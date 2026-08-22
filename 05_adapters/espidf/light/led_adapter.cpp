#include "05_adapters/espidf/light/led_adapter.hpp"

#include <algorithm>

#include "01_core/domain/sensing.hpp"
#include "driver/ledc.h"
#include "esp_timer.h"

namespace plant {
namespace {

constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_1;
constexpr ledc_channel_t kRed = LEDC_CHANNEL_1;
constexpr ledc_channel_t kGreen = LEDC_CHANNEL_2;
constexpr ledc_channel_t kBlue = LEDC_CHANNEL_3;

std::uint8_t triangle(
    std::uint64_t elapsed_us,
    std::uint64_t period_us,
    std::uint8_t maximum_duty) {
    const std::uint64_t position = elapsed_us % period_us;
    const std::uint64_t half = period_us / 2;
    const std::uint64_t value = position <= half ? position : period_us - position;
    return static_cast<std::uint8_t>(value * maximum_duty / half);
}

}  // namespace

EspLedAdapter::EspLedAdapter(EspLedConfig config) noexcept : config_(config) {}

Status EspLedAdapter::initialize() {  // RGB 三个通道共享一个 LEDC 定时器的频率和分辨率。
    ledc_timer_config_t timer{};
    timer.speed_mode = kMode;
    timer.duty_resolution = static_cast<ledc_timer_bit_t>(config_.duty_resolution_bits);
    timer.timer_num = kTimer;
    timer.freq_hz = config_.frequency_hz;
    // ESP32-C3 的所有 LEDC 定时器共享一个全局时钟源；与舵机、振动统一使用 XTAL。
    timer.clk_cfg = LEDC_USE_XTAL_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        return Status::failure(ErrorCode::LightingFailure);
    }

    // GPIO 数组与 LEDC 通道数组按 RGB 顺序一一对应；颜色由三路占空比的比例决定，
    // 不是由 PWM 频率混合产生。
    const int pins[]{config_.red_gpio, config_.green_gpio, config_.blue_gpio};
    const ledc_channel_t channels[]{kRed, kGreen, kBlue};
    for (int index = 0; index < 3; ++index) {
        ledc_channel_config_t channel{};
        channel.gpio_num = pins[index];
        channel.speed_mode = kMode;
        channel.channel = channels[index];
        channel.timer_sel = kTimer;
        // 初始化阶段保持熄灭；呼吸波形由 tick() 计算，并由 set_rgb() 更新占空比。
        channel.duty = 0;
        if (ledc_channel_config(&channel) != ESP_OK) {
            return Status::failure(ErrorCode::LightingFailure);
        }
    }
    initialized_ = true;
    return stop();
}

Status EspLedAdapter::play(LightPattern pattern, std::uint32_t) {
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    pattern_ = pattern;
    started_us_ = static_cast<std::uint64_t>(esp_timer_get_time());
    return tick(started_us_);
}

Status EspLedAdapter::stop() {
    pattern_ = LightPattern::FadeOut;
    started_us_ = 0;
    return set_rgb(0, 0, 0);
}

Status EspLedAdapter::tick(std::uint64_t now_us) {
    const std::uint64_t elapsed = now_us - started_us_;
    switch (pattern_) {
        case LightPattern::FadeIn: {
            const auto duty = static_cast<std::uint8_t>(
                elapsed >= 600000 ? config_.maximum_duty
                                  : elapsed * config_.maximum_duty / 600000);
            return set_rgb(0, duty, duty / 3);
        }
        case LightPattern::SoftBreathing: {
            const auto duty = triangle(elapsed, 2400000, config_.maximum_duty);
            return set_rgb(0, duty, duty / 4);
        }
        case LightPattern::ShortPulse:
            return set_rgb(0, elapsed < 250000 ? config_.maximum_duty : 0, 0);
        case LightPattern::SlowBreathing: {
            const auto duty = triangle(elapsed, 4000000, config_.maximum_duty);
            return set_rgb(0, duty / 2, duty / 5);
        }
        case LightPattern::FadeOut: {
            const auto duty = static_cast<std::uint8_t>(
                elapsed >= 600000 ? 0
                                  : config_.maximum_duty -
                                        elapsed * config_.maximum_duty / 600000);
            return set_rgb(0, duty, duty / 3);
        }
        case LightPattern::ErrorBlink:
            return set_rgb(
                (elapsed / 500000) % 2 == 0 ? config_.maximum_duty : 0, 0, 0);
        case LightPattern::TouchPulse:
            return set_rgb(0, elapsed < 350000 ? config_.maximum_duty : 0, 0);
        case LightPattern::ListeningBreath: {
            const std::uint32_t scale = 250U + intensity_ * 750U / 1000U;
            const auto duty = static_cast<std::uint8_t>(
                triangle(elapsed, 1800000, config_.maximum_duty) * scale / 1000U);
            return set_rgb(0, duty, duty);
        }
        case LightPattern::SunGlow: {
            const std::uint32_t scale = 300U + intensity_ * 700U / 1000U;
            const auto duty = static_cast<std::uint8_t>(
                triangle(elapsed, 3600000, config_.maximum_duty) * scale / 1000U);
            return set_rgb(duty, static_cast<std::uint8_t>(duty * 2U / 3U), 0);
        }
        case LightPattern::ComfortGlow: {
            const auto duty = static_cast<std::uint8_t>(
                elapsed >= 800000 ? config_.maximum_duty
                                  : elapsed * config_.maximum_duty / 800000);
            return set_rgb(duty, duty / 2U, duty / 2U);
        }
        case LightPattern::GrowthRise: {
            const std::uint64_t phase = std::min<std::uint64_t>(elapsed, 1000000);
            const auto warm = static_cast<std::uint8_t>(
                phase * config_.maximum_duty / 1000000ULL);
            return set_rgb(warm, config_.maximum_duty, warm / 2U);
        }
        case LightPattern::GrowthLimit: {
            const bool on = elapsed < 120000 || (elapsed >= 220000 && elapsed < 340000);
            return set_rgb(
                on ? config_.maximum_duty : 0,
                on ? config_.maximum_duty : 0,
                on ? config_.maximum_duty / 2U : 0);
        }
    }
    return Status::failure(ErrorCode::LightingFailure);
}

Status EspLedAdapter::set_intensity(std::uint16_t intensity) {
    intensity_ = std::min(intensity, kNormalizedSensorMaximum);
    return Status::success();
}

Status EspLedAdapter::set_rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
    const std::uint32_t values[]{red, green, blue};
    const ledc_channel_t channels[]{kRed, kGreen, kBlue};
    for (int index = 0; index < 3; ++index) {
        const std::uint32_t maximum_hardware_duty =
            (1U << config_.duty_resolution_bits) - 1U;
        const std::uint32_t duty =
            config_.active_high ? values[index] : maximum_hardware_duty - values[index];
        if (ledc_set_duty(kMode, channels[index], duty) != ESP_OK ||
            ledc_update_duty(kMode, channels[index]) != ESP_OK) {
            return Status::failure(ErrorCode::LightingFailure);
        }
    }
    return Status::success();
}

}  // namespace plant
