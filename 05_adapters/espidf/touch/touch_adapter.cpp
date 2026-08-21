#include "05_adapters/espidf/touch/touch_adapter.hpp"

#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "driver/gpio.h"

namespace plant {

using Config = bsp::v1::BoardConfig;

Status EspTouchAdapter::initialize() {
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << Config::Gpio::touch_input;
    config.mode = GPIO_MODE_INPUT;
    config.pull_down_en = Config::Touch::enable_internal_pull_down ? GPIO_PULLDOWN_ENABLE
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
        now_ms - raw_changed_ms_ >= Config::Touch::debounce_ms) {
        stable_pressed_ = raw_pressed_;
        if (stable_pressed_) {
            pressed_since_ms_ = now_ms;
            long_press_reported_ = false;
        } else if (!long_press_reported_) {
            gesture = TouchGesture::SingleTap;
            return true;
        }
    }

    if (stable_pressed_ && !long_press_reported_ &&
        now_ms - pressed_since_ms_ >= Config::Touch::long_press_ms) {
        long_press_reported_ = true;
        gesture = TouchGesture::LongPress;
        return true;
    }
    return false;
}

bool EspTouchAdapter::read_pressed() const noexcept {
    const bool level = gpio_get_level(static_cast<gpio_num_t>(Config::Gpio::touch_input)) != 0;
    return Config::Touch::active_high ? level : !level;
}

}  // namespace plant
