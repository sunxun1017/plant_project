#include "05_adapters/espidf/touch/touch_adapter.hpp"

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

bool EspTouchAdapter::read_pressed() const noexcept {
    const bool level = gpio_get_level(static_cast<gpio_num_t>(config_.gpio)) != 0;
    return config_.active_high ? level : !level;
}

}  // namespace plant
