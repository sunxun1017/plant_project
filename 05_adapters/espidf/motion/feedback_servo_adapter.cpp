#include "05_adapters/espidf/motion/feedback_servo_adapter.hpp"

#include <algorithm>
#include <cstdlib>

#include "06_bsp/plant_v2/plant_v2_board.hpp"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"

namespace plant {
namespace {

using Config = bsp::v2::BoardConfig;
constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_0;

std::uint16_t normalize_position(int raw) noexcept {
    const int bounded = std::clamp(
        raw, Config::Position::valid_raw_minimum, Config::Position::valid_raw_maximum);
    const std::uint16_t normalized = static_cast<std::uint16_t>(
        (bounded - Config::Position::valid_raw_minimum) * kNormalizedSensorMaximum /
        (Config::Position::valid_raw_maximum - Config::Position::valid_raw_minimum));
    return Config::Position::increasing_raw_means_growing
               ? normalized
               : static_cast<std::uint16_t>(kNormalizedSensorMaximum - normalized);
}

}  // namespace

FeedbackServoAdapter::FeedbackServoAdapter(V2AdcSampler& adc) noexcept : adc_(adc) {}

Status FeedbackServoAdapter::initialize() {
    gpio_config_t power{};
    power.pin_bit_mask = 1ULL << Config::Gpio::servo_power_enable;
    power.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&power) != ESP_OK ||
        gpio_set_level(
            static_cast<gpio_num_t>(Config::Gpio::servo_power_enable),
            Config::Servo::power_enable_active_high ? 0 : 1) != ESP_OK) {
        return Status::failure(ErrorCode::MotionFailure);
    }

    ledc_timer_config_t timer{};
    timer.speed_mode = kSpeedMode;
    timer.duty_resolution =
        static_cast<ledc_timer_bit_t>(Config::Servo::duty_resolution_bits);
    timer.timer_num = kTimer;
    timer.freq_hz = Config::Servo::frequency_hz;
    timer.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        return Status::failure(ErrorCode::MotionFailure);
    }

    ledc_channel_config_t channel{};
    channel.gpio_num = Config::Gpio::servo_pwm;
    channel.speed_mode = kSpeedMode;
    channel.channel = kChannel;
    channel.timer_sel = kTimer;
    channel.duty = 0;
    channel.hpoint = 0;
    if (ledc_channel_config(&channel) != ESP_OK) {
        return Status::failure(ErrorCode::MotionFailure);
    }

    initialized_ = true;
    const Status position_status = sample_position(
        static_cast<std::uint64_t>(esp_timer_get_time()));
    if (!position_status.ok()) {
        initialized_ = false;
        return position_status;
    }
    snapshot_.target_position = snapshot_.actual_position;
    snapshot_.target_reached = true;
    return Status::success();
}

Status FeedbackServoAdapter::play(MotionPattern pattern, std::uint32_t execution_id) {
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (pattern == MotionPattern::StopAndHoldSafe) {
        // 错误行为只停止输出，不清除导致它的反馈/卡滞诊断；下一次成功启动运动才清除。
        return stop();
    }
    pattern_ = pattern;
    absolute_move_ = false;
    phase_ = 0;
    execution_id_ = execution_id;
    return start_target(target_for_phase());
}

Status FeedbackServoAdapter::move_to(
    std::uint16_t target_position,
    std::uint32_t execution_id) {
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    absolute_move_ = true;
    phase_ = 0;
    execution_id_ = execution_id;
    return start_target(target_position);
}

Status FeedbackServoAdapter::stop() {
    snapshot_.moving = false;
    expected_direction_ = 0;
    const esp_err_t pwm_status = ledc_stop(kSpeedMode, kChannel, 0);
    const esp_err_t power_status = gpio_set_level(
        static_cast<gpio_num_t>(Config::Gpio::servo_power_enable),
        Config::Servo::power_enable_active_high ? 0 : 1);
    return pwm_status == ESP_OK && power_status == ESP_OK
               ? Status::success()
               : Status::failure(ErrorCode::MotionFailure);
}

Status FeedbackServoAdapter::poll(std::uint64_t now_us, MotionPollResult& result) {
    result = MotionPollResult{};
    if (!initialized_ || now_us < next_sample_us_) {
        return initialized_ ? Status::success() : Status::failure(ErrorCode::InvalidState);
    }
    const Status sample_status = sample_position(now_us);
    if (!sample_status.ok()) {
        return snapshot_.moving ? fail_motion(MotionFault::FeedbackInvalid) : sample_status;
    }
    if (!snapshot_.moving) {
        return Status::success();
    }

    const std::int32_t error = static_cast<std::int32_t>(snapshot_.target_position) -
                               snapshot_.actual_position;
    if (std::abs(error) <= Config::Position::tolerance) {
        ++stable_samples_;
        if (stable_samples_ < Config::Position::stable_sample_count) {
            return Status::success();
        }

        ++phase_;
        if (!absolute_move_ && phase_ < phase_count()) {
            return start_target(target_for_phase());
        }
        snapshot_.moving = false;
        snapshot_.target_reached = true;
        expected_direction_ = 0;
        result.completed = true;
        result.execution_id = execution_id_;
        if (!absolute_move_ && pattern_ == MotionPattern::MoveToSleepPose) {
            const Status stop_status = stop();
            if (!stop_status.ok()) {
                result.completed = false;
                return stop_status;
            }
        }
        return Status::success();
    }
    stable_samples_ = 0;

    const std::int32_t step = static_cast<std::int32_t>(snapshot_.actual_position) -
                              last_position_;
    if (expected_direction_ != 0 && step * expected_direction_ <=
                                         -static_cast<std::int32_t>(
                                             Config::Position::minimum_progress)) {
        ++opposite_samples_;
        if (opposite_samples_ >= 3) {
            return fail_motion(MotionFault::OppositeDirection);
        }
    } else if (step * expected_direction_ > 0) {
        opposite_samples_ = 0;
    }
    last_position_ = snapshot_.actual_position;

    const std::int32_t progress =
        (static_cast<std::int32_t>(snapshot_.actual_position) - progress_position_) *
        expected_direction_;
    if (progress >= Config::Position::minimum_progress) {
        progress_position_ = snapshot_.actual_position;
        last_progress_us_ = now_us;
    } else if (now_us - last_progress_us_ >=
               Config::Position::no_progress_timeout_ms * 1000ULL) {
        return fail_motion(MotionFault::Stalled);
    }
    if (now_us - motion_started_us_ >= Config::Position::motion_timeout_ms * 1000ULL) {
        return fail_motion(MotionFault::Timeout);
    }
    return Status::success();
}

PositionSnapshot FeedbackServoAdapter::position_snapshot() const noexcept {
    return snapshot_;
}

Status FeedbackServoAdapter::start_target(std::uint16_t target_position) {
    if (snapshot_.feedback != PositionFeedbackState::Valid) {
        return Status::failure(ErrorCode::MotionFailure);
    }
    target_position = std::clamp(
        target_position,
        Config::Position::safe_minimum,
        Config::Position::safe_maximum);
    if (gpio_set_level(
            static_cast<gpio_num_t>(Config::Gpio::servo_power_enable),
            Config::Servo::power_enable_active_high ? 1 : 0) != ESP_OK) {
        return Status::failure(ErrorCode::MotionFailure);
    }
    const Status pwm_status = set_target_pwm(target_position);
    if (!pwm_status.ok()) {
        (void)stop();
        return pwm_status;
    }

    snapshot_.target_position = target_position;
    snapshot_.target_reached = false;
    snapshot_.moving = true;
    snapshot_.fault = MotionFault::None;
    stable_samples_ = 0;
    opposite_samples_ = 0;
    last_position_ = snapshot_.actual_position;
    progress_position_ = snapshot_.actual_position;
    expected_direction_ = target_position > snapshot_.actual_position
                              ? 1
                              : (target_position < snapshot_.actual_position ? -1 : 0);
    motion_started_us_ = static_cast<std::uint64_t>(esp_timer_get_time());
    last_progress_us_ = motion_started_us_;
    next_sample_us_ = motion_started_us_;
    return Status::success();
}

Status FeedbackServoAdapter::set_target_pwm(std::uint16_t target_position) {
    const std::uint32_t pulse_range =
        Config::Servo::maximum_pulse_us - Config::Servo::minimum_pulse_us;
    const std::uint32_t pulse_us = Config::Servo::minimum_pulse_us +
                                   target_position * pulse_range /
                                       kNormalizedSensorMaximum;
    constexpr std::uint32_t maximum_duty =
        (1U << Config::Servo::duty_resolution_bits) - 1U;
    constexpr std::uint32_t period_us = 1000000U / Config::Servo::frequency_hz;
    const std::uint32_t duty = pulse_us * maximum_duty / period_us;
    return ledc_set_duty(kSpeedMode, kChannel, duty) == ESP_OK &&
                   ledc_update_duty(kSpeedMode, kChannel) == ESP_OK
               ? Status::success()
               : Status::failure(ErrorCode::MotionFailure);
}

Status FeedbackServoAdapter::sample_position(std::uint64_t now_us) {
    int raw = 0;
    const Status status = adc_.read_raw(
        static_cast<adc_channel_t>(Config::Adc::position_channel), raw);
    next_sample_us_ = now_us + Config::Position::sample_period_ms * 1000ULL;
    if (!status.ok()) {
        snapshot_.feedback = PositionFeedbackState::Unavailable;
        return status;
    }
    if (raw <= Config::Position::rail_low_raw_maximum) {
        snapshot_.feedback = PositionFeedbackState::ShortCircuit;
        return Status::failure(ErrorCode::MotionFailure);
    }
    if (raw >= Config::Position::rail_high_raw_minimum) {
        snapshot_.feedback = PositionFeedbackState::OpenCircuit;
        return Status::failure(ErrorCode::MotionFailure);
    }
    if (raw < Config::Position::valid_raw_minimum ||
        raw > Config::Position::valid_raw_maximum) {
        snapshot_.feedback = PositionFeedbackState::OutOfRange;
        return Status::failure(ErrorCode::MotionFailure);
    }
    snapshot_.actual_position = normalize_position(raw);
    snapshot_.feedback = PositionFeedbackState::Valid;
    return Status::success();
}

Status FeedbackServoAdapter::fail_motion(MotionFault fault) {
    snapshot_.fault = fault;
    snapshot_.target_reached = false;
    (void)stop();
    return Status::failure(ErrorCode::MotionFailure);
}

std::uint16_t FeedbackServoAdapter::target_for_phase() const noexcept {
    switch (pattern_) {
        case MotionPattern::Wake:
        case MotionPattern::ReturnNeutral:
            return Config::Position::neutral;
        case MotionPattern::GentleSway:
            if (phase_ == 0) {
                return Config::Position::sway_right;
            }
            return phase_ == 1 ? Config::Position::sway_left
                               : Config::Position::neutral;
        case MotionPattern::LookUp:
            return phase_ == 0 ? Config::Position::look_up : Config::Position::neutral;
        case MotionPattern::MoveToSleepPose:
            return Config::Position::sleep;
        case MotionPattern::StopAndHoldSafe:
            return snapshot_.actual_position;
    }
    return snapshot_.actual_position;
}

std::uint8_t FeedbackServoAdapter::phase_count() const noexcept {
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
