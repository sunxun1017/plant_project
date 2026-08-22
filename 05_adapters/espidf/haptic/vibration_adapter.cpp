#include "05_adapters/espidf/haptic/vibration_adapter.hpp"

#include "driver/ledc.h"
#include "esp_timer.h"

namespace plant {
namespace {

constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_2;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_4;

}  // namespace

EspVibrationAdapter::EspVibrationAdapter(EspVibrationConfig config) noexcept
    : config_(config) {}

Status EspVibrationAdapter::initialize() {
    ledc_timer_config_t timer{};
    timer.speed_mode = kMode;
    timer.duty_resolution =
        static_cast<ledc_timer_bit_t>(config_.duty_resolution_bits);
    timer.timer_num = kTimer;
    timer.freq_hz = config_.frequency_hz;
    // ESP32-C3 的所有 LEDC 定时器共享一个全局时钟源；与舵机、RGB 统一使用 XTAL。
    timer.clk_cfg = LEDC_USE_XTAL_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        return Status::failure(ErrorCode::HapticFailure);
    }

    ledc_channel_config_t channel{};
    channel.gpio_num = config_.gpio;
    channel.speed_mode = kMode;
    channel.channel = kChannel;
    channel.timer_sel = kTimer;
    channel.duty = 0;
    if (ledc_channel_config(&channel) != ESP_OK) {
        return Status::failure(ErrorCode::HapticFailure);
    }
    initialized_ = true;
    return stop();
}

Status EspVibrationAdapter::play(HapticPattern pattern, std::uint32_t) {
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    pattern_ = pattern;
    started_us_ = static_cast<std::uint64_t>(esp_timer_get_time());
    return tick(started_us_);
}

Status EspVibrationAdapter::stop() {
    pattern_ = HapticPattern::Off;
    return set_duty(0);
}

Status EspVibrationAdapter::tick(std::uint64_t now_us) {
    const std::uint64_t elapsed = now_us - started_us_;
    switch (pattern_) {
        case HapticPattern::Off:
            return set_duty(0);
        case HapticPattern::SoftPulse:
            return set_duty(elapsed < 150000 ? config_.soft_duty : 0);
        case HapticPattern::DoubleSoftPulse:
            return set_duty(
                (elapsed < 120000 || (elapsed >= 240000 && elapsed < 360000))
                    ? config_.soft_duty
                    : 0);
        case HapticPattern::Warning:
            return set_duty(elapsed < config_.maximum_continuous_time_ms * 1000ULL
                                ? config_.warning_duty
                                : 0);
    }
    return Status::failure(ErrorCode::HapticFailure);
}

Status EspVibrationAdapter::set_duty(std::uint8_t duty) {
    const std::uint32_t maximum_hardware_duty =
        (1U << config_.duty_resolution_bits) - 1U;
    const std::uint32_t output =
        config_.active_high ? duty : maximum_hardware_duty - duty;
    if (ledc_set_duty(kMode, kChannel, output) != ESP_OK ||
        ledc_update_duty(kMode, kChannel) != ESP_OK) {
        return Status::failure(ErrorCode::HapticFailure);
    }
    return Status::success();
}

}  // namespace plant
