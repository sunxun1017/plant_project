#include "05_adapters/espidf/power/esp_power_adapter.hpp"

#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "driver/gpio.h"
#include "esp_sleep.h"

namespace plant {

using Config = bsp::v1::BoardConfig;

Status EspPowerAdapter::enter_light_sleep() {
    const Status wake_status = configure_touch_wakeup(false);
    if (!wake_status.ok()) {
        return wake_status;
    }
    const std::uint64_t deep_sleep_delay_us =
        static_cast<std::uint64_t>(Config::Power::deep_sleep_delay_ms) * 1000ULL;
    if (Config::Power::deep_sleep_enabled && deep_sleep_delay_us != 0 &&
        esp_sleep_enable_timer_wakeup(deep_sleep_delay_us) != ESP_OK) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    return esp_light_sleep_start() == ESP_OK ? Status::success()
                                              : Status::failure(ErrorCode::InternalFailure);
}

Status EspPowerAdapter::enter_deep_sleep(std::uint64_t timer_wakeup_us) {
    const Status wake_status = configure_touch_wakeup(true);
    if (!wake_status.ok()) {
        return wake_status;
    }
    if (timer_wakeup_us != 0 && esp_sleep_enable_timer_wakeup(timer_wakeup_us) != ESP_OK) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    esp_deep_sleep_start();
    return Status::failure(ErrorCode::InternalFailure);
}

WakeSource EspPowerAdapter::wake_source() const {
    const std::uint32_t causes = esp_sleep_get_wakeup_causes();
    if ((causes & (1U << ESP_SLEEP_WAKEUP_GPIO)) != 0 ||
        (causes & (1U << ESP_SLEEP_WAKEUP_EXT1)) != 0) {
        return WakeSource::Touch;
    }
    if ((causes & (1U << ESP_SLEEP_WAKEUP_TIMER)) != 0) {
        return WakeSource::Timer;
    }
    if ((causes & (1U << ESP_SLEEP_WAKEUP_BT)) != 0) {
        return WakeSource::Ble;
    }
    return causes == 0 ? WakeSource::PowerOn : WakeSource::Unknown;
}

Status EspPowerAdapter::configure_touch_wakeup(bool deep_sleep) {
    const auto pin = static_cast<gpio_num_t>(Config::Gpio::touch_input);
    const gpio_int_type_t interrupt =
        Config::Touch::active_high ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
    if (!deep_sleep) {
        if (gpio_wakeup_enable(pin, interrupt) != ESP_OK ||
            esp_sleep_enable_gpio_wakeup() != ESP_OK) {
            return Status::failure(ErrorCode::InternalFailure);
        }
        return Status::success();
    }

    const auto mode = Config::Touch::active_high ? ESP_GPIO_WAKEUP_GPIO_HIGH
                                                 : ESP_GPIO_WAKEUP_GPIO_LOW;
    return esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(
               1ULL << Config::Gpio::touch_input, mode) == ESP_OK
               ? Status::success()
               : Status::failure(ErrorCode::InternalFailure);
}

}  // namespace plant
