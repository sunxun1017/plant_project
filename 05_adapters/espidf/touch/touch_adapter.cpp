#include "05_adapters/espidf/touch/touch_adapter.hpp"

#include <algorithm>

#include "driver/gpio.h"

namespace plant {

EspTouchAdapter::EspTouchAdapter(EspTouchConfig config) noexcept : config_(config) {}

Status EspTouchAdapter::initialize() {
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << config_.gpio;
    config.mode = GPIO_MODE_INPUT;
    config.pull_down_en = config_.enable_internal_pull_down ? GPIO_PULLDOWN_ENABLE
                                                            : GPIO_PULLDOWN_DISABLE;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&config) != ESP_OK) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    raw_pressed_ = read_pressed();
    stable_pressed_ = raw_pressed_;
    if (config_.runtime_event.configured()) {
        const esp_err_t service_status = gpio_install_isr_service(0);
        if (service_status != ESP_OK && service_status != ESP_ERR_INVALID_STATE) {
            return Status::failure(ErrorCode::InternalFailure);
        }
        if (gpio_isr_handler_add(
                static_cast<gpio_num_t>(config_.gpio), gpio_interrupt, this) != ESP_OK) {
            return Status::failure(ErrorCode::InternalFailure);
        }
        const Status interrupt_status = arm_interrupt_for_next_level(raw_pressed_);
        if (!interrupt_status.ok()) {
            return interrupt_status;
        }
    }
    initialized_ = true;
    return Status::success();
}

bool EspTouchAdapter::poll(std::uint64_t now_ms, TouchGesture& gesture) {
    if (!initialized_) {
        return false;
    }

    const bool current = read_pressed();
    if (current != raw_pressed_) {
        raw_pressed_ = current;
        raw_changed_ms_ = now_ms;
        if (config_.runtime_event.configured()) {
            (void)arm_interrupt_for_next_level(current);
        }
    } else if (config_.runtime_event.configured() &&
               !interrupt_armed_.load(std::memory_order_relaxed)) {
        (void)arm_interrupt_for_next_level(current);
    }

    if (raw_pressed_ != stable_pressed_ &&
        now_ms - raw_changed_ms_ >= config_.debounce_ms) {
        stable_pressed_ = raw_pressed_;
        if (stable_pressed_) {
            pressed_since_ms_ = now_ms;
            long_press_reported_ = false;
            factory_reset_reported_ = false;
        } else if (!long_press_reported_) {
            gesture = TouchGesture::SingleTap;
            return true;
        }
    }

    if (stable_pressed_ && config_.factory_reset_hold_ms != 0 &&
        !factory_reset_reported_ &&
        now_ms - pressed_since_ms_ >= config_.factory_reset_hold_ms) {
        factory_reset_reported_ = true;
        long_press_reported_ = true;
        gesture = TouchGesture::FactoryResetHold;
        return true;
    }
    if (stable_pressed_ && !long_press_reported_ &&
        now_ms - pressed_since_ms_ >= config_.long_press_ms) {
        long_press_reported_ = true;
        gesture = TouchGesture::LongPress;
        return true;
    }
    return false;
}

std::uint32_t EspTouchAdapter::next_poll_delay_ms(
    std::uint64_t now_ms,
    std::uint32_t maximum_delay_ms) const noexcept {
    if (!initialized_ || maximum_delay_ms == 0) {
        return maximum_delay_ms;
    }

    std::uint64_t deadline_ms = now_ms + maximum_delay_ms;
    if (raw_pressed_ != stable_pressed_) {
        deadline_ms = std::min(deadline_ms, raw_changed_ms_ + config_.debounce_ms);
    } else if (stable_pressed_) {
        if (!long_press_reported_) {
            deadline_ms = std::min(deadline_ms, pressed_since_ms_ + config_.long_press_ms);
        }
        if (config_.factory_reset_hold_ms != 0 && !factory_reset_reported_) {
            deadline_ms =
                std::min(deadline_ms, pressed_since_ms_ + config_.factory_reset_hold_ms);
        }
    }
    if (deadline_ms <= now_ms) {
        return 1;
    }
    return static_cast<std::uint32_t>(deadline_ms - now_ms);
}

void EspTouchAdapter::gpio_interrupt(void* argument) {
    auto* self = static_cast<EspTouchAdapter*>(argument);
    if (self == nullptr) {
        return;
    }
    (void)gpio_intr_disable(static_cast<gpio_num_t>(self->config_.gpio));
    self->interrupt_armed_.store(false, std::memory_order_relaxed);
    self->config_.runtime_event.notify(true);
}

Status EspTouchAdapter::arm_interrupt_for_next_level(bool currently_pressed) {
    const gpio_int_type_t press_level =
        config_.active_high ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
    const gpio_int_type_t release_level =
        config_.active_high ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    const auto pin = static_cast<gpio_num_t>(config_.gpio);
    if (gpio_set_intr_type(pin, currently_pressed ? release_level : press_level) != ESP_OK ||
        gpio_intr_enable(pin) != ESP_OK) {
        interrupt_armed_.store(false, std::memory_order_relaxed);
        return Status::failure(ErrorCode::InternalFailure);
    }
    interrupt_armed_.store(true, std::memory_order_relaxed);
    return Status::success();
}

bool EspTouchAdapter::read_pressed() const noexcept {
    const bool level = gpio_get_level(static_cast<gpio_num_t>(config_.gpio)) != 0;
    return config_.active_high ? level : !level;
}

}  // namespace plant
