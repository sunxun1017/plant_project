#include "05_adapters/espidf/haptic/vibration_adapter.hpp"

#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "driver/ledc.h"
#include "esp_timer.h"

namespace plant {
namespace {

using Config = bsp::v1::BoardConfig;
constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_2;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_4;

}  // namespace

Status EspVibrationAdapter::initialize() {
    ledc_timer_config_t timer{};
    timer.speed_mode = kMode;
    timer.duty_resolution =
        static_cast<ledc_timer_bit_t>(Config::Vibration::duty_resolution_bits);
    timer.timer_num = kTimer;
    timer.freq_hz = Config::Vibration::frequency_hz;
    timer.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        return Status::failure(ErrorCode::HapticFailure);
    }

    ledc_channel_config_t channel{};
    channel.gpio_num = Config::Gpio::vibration_pwm;
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
            return set_duty(elapsed < 150000 ? Config::Vibration::soft_duty : 0);
        case HapticPattern::DoubleSoftPulse:
            return set_duty(
                (elapsed < 120000 || (elapsed >= 240000 && elapsed < 360000))
                    ? Config::Vibration::soft_duty
                    : 0);
        case HapticPattern::Warning:
            return set_duty(elapsed < Config::Vibration::maximum_continuous_time_ms * 1000ULL
                                ? Config::Vibration::warning_duty
                                : 0);
    }
    return Status::failure(ErrorCode::HapticFailure);
}

Status EspVibrationAdapter::set_duty(std::uint8_t duty) {
    const std::uint32_t output = Config::Vibration::active_high ? duty : 255 - duty;
    if (ledc_set_duty(kMode, kChannel, output) != ESP_OK ||
        ledc_update_duty(kMode, kChannel) != ESP_OK) {
        return Status::failure(ErrorCode::HapticFailure);
    }
    return Status::success();
}

}  // namespace plant
