#include "05_adapters/espidf/light/led_adapter.hpp"

#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "driver/ledc.h"
#include "esp_timer.h"

namespace plant {
namespace {

using Config = bsp::v1::BoardConfig;
constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_1;
constexpr ledc_channel_t kRed = LEDC_CHANNEL_1;
constexpr ledc_channel_t kGreen = LEDC_CHANNEL_2;
constexpr ledc_channel_t kBlue = LEDC_CHANNEL_3;

std::uint8_t triangle(std::uint64_t elapsed_us, std::uint64_t period_us) {
    const std::uint64_t position = elapsed_us % period_us;
    const std::uint64_t half = period_us / 2;
    const std::uint64_t value = position <= half ? position : period_us - position;
    return static_cast<std::uint8_t>(value * Config::Led::maximum_duty / half);
}

}  // namespace

Status EspLedAdapter::initialize() {
    ledc_timer_config_t timer{};
    timer.speed_mode = kMode;
    timer.duty_resolution = static_cast<ledc_timer_bit_t>(Config::Led::duty_resolution_bits);
    timer.timer_num = kTimer;
    timer.freq_hz = Config::Led::frequency_hz;
    timer.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        return Status::failure(ErrorCode::LightingFailure);
    }

    const int pins[]{Config::Gpio::led_red_pwm, Config::Gpio::led_green_pwm,
                     Config::Gpio::led_blue_pwm};
    const ledc_channel_t channels[]{kRed, kGreen, kBlue};
    for (int index = 0; index < 3; ++index) {
        ledc_channel_config_t channel{};
        channel.gpio_num = pins[index];
        channel.speed_mode = kMode;
        channel.channel = channels[index];
        channel.timer_sel = kTimer;
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
    tick(started_us_);
    return Status::success();
}

Status EspLedAdapter::stop() {
    pattern_ = LightPattern::FadeOut;
    started_us_ = 0;
    return set_rgb(0, 0, 0);
}

void EspLedAdapter::tick(std::uint64_t now_us) {
    const std::uint64_t elapsed = now_us - started_us_;
    switch (pattern_) {
        case LightPattern::FadeIn: {
            const auto duty = static_cast<std::uint8_t>(
                elapsed >= 600000 ? Config::Led::maximum_duty
                                  : elapsed * Config::Led::maximum_duty / 600000);
            (void)set_rgb(0, duty, duty / 3);
            break;
        }
        case LightPattern::SoftBreathing: {
            const auto duty = triangle(elapsed, 2400000);
            (void)set_rgb(0, duty, duty / 4);
            break;
        }
        case LightPattern::ShortPulse:
            (void)set_rgb(0, elapsed < 250000 ? Config::Led::maximum_duty : 0, 0);
            break;
        case LightPattern::SlowBreathing: {
            const auto duty = triangle(elapsed, 4000000);
            (void)set_rgb(0, duty / 2, duty / 5);
            break;
        }
        case LightPattern::FadeOut: {
            const auto duty = static_cast<std::uint8_t>(
                elapsed >= 600000 ? 0
                                  : Config::Led::maximum_duty -
                                        elapsed * Config::Led::maximum_duty / 600000);
            (void)set_rgb(0, duty, duty / 3);
            break;
        }
        case LightPattern::ErrorBlink:
            (void)set_rgb((elapsed / 500000) % 2 == 0 ? Config::Led::maximum_duty : 0, 0, 0);
            break;
    }
}

Status EspLedAdapter::set_rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
    const std::uint32_t values[]{red, green, blue};
    const ledc_channel_t channels[]{kRed, kGreen, kBlue};
    for (int index = 0; index < 3; ++index) {
        const std::uint32_t duty = Config::Led::active_high ? values[index] : 255 - values[index];
        if (ledc_set_duty(kMode, channels[index], duty) != ESP_OK ||
            ledc_update_duty(kMode, channels[index]) != ESP_OK) {
            return Status::failure(ErrorCode::LightingFailure);
        }
    }
    return Status::success();
}

}  // namespace plant
