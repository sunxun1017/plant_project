#include "05_adapters/espidf/motion/servo_adapter.hpp"

#include <algorithm>

#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"

namespace plant {
namespace {

using Config = bsp::v1::BoardConfig;
constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_0;

}  // namespace

Status EspServoAdapter::initialize() {  // 配置舵机电源使能 GPIO 和 LEDC PWM 输出。
    gpio_config_t power{};  // 舵机电源使能引脚，不是多级电源控制器。
    power.pin_bit_mask = 1ULL << Config::Gpio::servo_power_enable;
    power.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&power) != ESP_OK) {
        return Status::failure(ErrorCode::MotionFailure);
    }
    // 初始化时按板级有效电平关闭舵机电源。
    // TODO: 检查 gpio_set_level() 的返回值，避免电平设置失败却继续初始化。
    gpio_set_level(
        static_cast<gpio_num_t>(Config::Gpio::servo_power_enable),
        Config::Servo::power_enable_active_high ? 0 : 1);

    ledc_timer_config_t timer{};  // LEDC 定时器提供 PWM 频率和占空比分辨率。
    timer.speed_mode = kSpeedMode;  // ESP32-C3 使用 LEDC 低速模式。
    timer.duty_resolution = static_cast<ledc_timer_bit_t>(Config::Servo::duty_resolution_bits); // 当前板级配置为 14 位分辨率。
    timer.timer_num = kTimer;
    timer.freq_hz = Config::Servo::frequency_hz;  // 当前板级配置为舵机常用的 50 Hz。
    timer.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        return Status::failure(ErrorCode::MotionFailure);
    }
    /*
     LEDC Timer
      │ 提供频率和分辨率
      ▼
     LEDC Channel
        │ 提供占空比
        ▼
     GPIO Matrix
        │ 把内部信号路由出去
        ▼
    具体 GPIO 引脚
     */
    ledc_channel_config_t channel{};  // 通道是 LEDC 内部 PWM 输出通路。
    // GPIO Matrix 把 LEDC 通道 0 路由到 BSP 指定的舵机 PWM 引脚。
    channel.gpio_num = Config::Gpio::servo_pwm;
    channel.speed_mode = kSpeedMode;
    channel.channel = kChannel;  // 当前使用 LEDC 通道 0。
    channel.timer_sel = kTimer;  // 当前通道使用 LEDC 定时器 0。
    channel.duty = 0;  // 初始化时占空比为 0，尚未输出有效舵机脉冲。
    channel.hpoint = 0;
    if (ledc_channel_config(&channel) != ESP_OK) {
        return Status::failure(ErrorCode::MotionFailure);
    }
    initialized_ = true;
    return Status::success();
}

Status EspServoAdapter::play(MotionPattern pattern, std::uint32_t execution_id) {
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    pattern_ = pattern;
    phase_ = 0;
    execution_id_ = execution_id;
    active_ = true;
    gpio_set_level(
        static_cast<gpio_num_t>(Config::Gpio::servo_power_enable),
        Config::Servo::power_enable_active_high ? 1 : 0);
    const Status status = set_pulse(pulse_for_phase());
    if (!status.ok()) {
        active_ = false;
        return status;
    }
    deadline_us_ = static_cast<std::uint64_t>(esp_timer_get_time()) +
                   Config::Servo::estimated_move_time_ms * 1000ULL;
    return Status::success();
}

Status EspServoAdapter::stop() {
    active_ = false;
    const esp_err_t pwm_status = ledc_stop(kSpeedMode, kChannel, 0);
    gpio_set_level(
        static_cast<gpio_num_t>(Config::Gpio::servo_power_enable),
        Config::Servo::power_enable_active_high ? 0 : 1);
    return pwm_status == ESP_OK ? Status::success()
                                : Status::failure(ErrorCode::MotionFailure);
}

Status EspServoAdapter::poll(std::uint64_t now_us, MotionPollResult& result) {
    result = MotionPollResult{};
    if (!active_ || now_us < deadline_us_) {
        return Status::success();
    }
    ++phase_;
    if (phase_ < phase_count()) {
        const Status pulse_status = set_pulse(pulse_for_phase());
        if (!pulse_status.ok()) {
            active_ = false;
            (void)stop();
            return pulse_status;
        }
        deadline_us_ = now_us + Config::Servo::estimated_move_time_ms * 1000ULL;
        return Status::success();
    }

    active_ = false;
    result.completed = true;
    result.execution_id = execution_id_;
    if (pattern_ == MotionPattern::MoveToSleepPose) {
        const Status stop_status = stop();
        if (!stop_status.ok()) {
            result.completed = false;
            return stop_status;
        }
    }
    return Status::success();
}

Status EspServoAdapter::set_pulse(std::uint16_t pulse_us) {
    pulse_us = std::clamp(
        pulse_us,
        Config::Servo::minimum_pulse_us,
        Config::Servo::maximum_pulse_us);
    constexpr std::uint32_t max_duty =
        (1U << Config::Servo::duty_resolution_bits) - 1U;
    constexpr std::uint32_t period_us = 1000000U / Config::Servo::frequency_hz;
    const std::uint32_t duty =
        static_cast<std::uint32_t>(pulse_us) * max_duty / period_us;
    if (ledc_set_duty(kSpeedMode, kChannel, duty) != ESP_OK ||
        ledc_update_duty(kSpeedMode, kChannel) != ESP_OK) {
        return Status::failure(ErrorCode::MotionFailure);
    }
    return Status::success();
}

std::uint16_t EspServoAdapter::pulse_for_phase() const noexcept {
    switch (pattern_) {
        case MotionPattern::Wake:
        case MotionPattern::ReturnNeutral:
        case MotionPattern::StopAndHoldSafe:
            return Config::Servo::neutral_pulse_us;
        case MotionPattern::GentleSway:
            if (phase_ == 0) {
                return Config::Servo::sway_right_pulse_us;
            }
            return phase_ == 1 ? Config::Servo::sway_left_pulse_us
                               : Config::Servo::neutral_pulse_us;
        case MotionPattern::LookUp:
            return phase_ == 0 ? Config::Servo::look_up_pulse_us
                               : Config::Servo::neutral_pulse_us;
        case MotionPattern::MoveToSleepPose:
            return Config::Servo::sleep_pulse_us;
    }
    return Config::Servo::neutral_pulse_us;
}

std::uint8_t EspServoAdapter::phase_count() const noexcept {
    switch (pattern_) {
        case MotionPattern::GentleSway:
            return 3;
        case MotionPattern::LookUp:
            return 2;
        default:
            return 1;
    }
}

}  // namespace plant
