#include "05_adapters/espidf/power/esp_power_adapter.hpp"

#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "driver/gpio.h"
#include "esp_pm.h"
#include "esp_sleep.h"

namespace plant {

using V1Config = bsp::v1::BoardConfig;

EspPowerAdapter::EspPowerAdapter() noexcept
    : EspPowerAdapter(EspPowerConfig{
          V1Config::Gpio::touch_input,
          V1Config::Touch::active_high,
          V1Config::Power::maximum_cpu_frequency_mhz,
          V1Config::Power::minimum_cpu_frequency_mhz,
      }) {}

EspPowerAdapter::EspPowerAdapter(EspPowerConfig config) noexcept : config_(config) {}

Status EspPowerAdapter::initialize() {
    const Status wake_status = configure_touch_wakeup(false);
    if (!wake_status.ok()) {
        return wake_status;
    }

    esp_pm_config_t configuration{};
    configuration.max_freq_mhz = config_.maximum_cpu_frequency_mhz;
    configuration.min_freq_mhz = config_.minimum_cpu_frequency_mhz;
    configuration.light_sleep_enable = true;
    if (esp_pm_configure(&configuration) != ESP_OK) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "plant_active", &active_lock_) !=
            ESP_OK ||
        esp_pm_lock_acquire(active_lock_) != ESP_OK) {
        if (active_lock_ != nullptr) {
            (void)esp_pm_lock_delete(active_lock_);
            active_lock_ = nullptr;
        }
        return Status::failure(ErrorCode::InternalFailure);
    }
    active_lock_held_ = true;
    initialized_ = true;
    return Status::success();
}

Status EspPowerAdapter::enter_light_sleep() {
    if (!initialized_ || active_lock_ == nullptr) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (active_lock_held_ && esp_pm_lock_release(active_lock_) != ESP_OK) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    active_lock_held_ = false;
    return Status::success();
}

Status EspPowerAdapter::leave_light_sleep() {
    if (!initialized_ || active_lock_ == nullptr) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (!active_lock_held_ && esp_pm_lock_acquire(active_lock_) != ESP_OK) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    active_lock_held_ = true;
    return Status::success();
}

Status EspPowerAdapter::enter_deep_sleep(std::uint64_t timer_wakeup_us) {
    if (esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL) != ESP_OK) {
        return Status::failure(ErrorCode::InternalFailure);
    }
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
    const auto pin = static_cast<gpio_num_t>(config_.touch_gpio);
    const gpio_int_type_t interrupt =
        config_.touch_active_high ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
    if (!deep_sleep) {
        if (gpio_wakeup_enable(pin, interrupt) != ESP_OK ||
            esp_sleep_enable_gpio_wakeup() != ESP_OK) {
            return Status::failure(ErrorCode::InternalFailure);
        }
        return Status::success();
    }

    const auto mode = config_.touch_active_high ? ESP_GPIO_WAKEUP_GPIO_HIGH
                                                : ESP_GPIO_WAKEUP_GPIO_LOW;
    return esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(
               1ULL << config_.touch_gpio, mode) == ESP_OK
               ? Status::success()
               : Status::failure(ErrorCode::InternalFailure);
}

}  // namespace plant
