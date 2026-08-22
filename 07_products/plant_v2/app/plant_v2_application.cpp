#include "07_products/plant_v2/app/plant_v2_application.hpp"

#include <limits>

namespace plant {

PlantV2Application::PlantV2Application(
    PlantApplication& base,
    LifecycleService& lifecycle,
    BehaviorService& behavior,
    OtaService& ota,
    IPositionMotionPort& motion,
    IHapticPort& haptic,
    IAcousticSensorPort& acoustic_port,
    IIlluminationSensorPort& illumination_port,
    IClimateSensorPort& climate_port,
    IBatterySensorPort& battery_port,
    AcousticService& acoustic,
    IlluminationService& illumination,
    ClimateService& climate,
    BatteryService& battery,
    GrowthService& growth,
    LightArbitrationService& light,
    std::uint64_t actuator_recovery_us) noexcept
    : base_(base),
      lifecycle_(lifecycle),
      behavior_(behavior),
      ota_(ota),
      motion_(motion),
      haptic_(haptic),
      acoustic_port_(acoustic_port),
      illumination_port_(illumination_port),
      climate_port_(climate_port),
      battery_port_(battery_port),
      acoustic_(acoustic),
      illumination_(illumination),
      climate_(climate),
      battery_(battery),
      growth_(growth),
      light_(light),
      actuator_recovery_us_(actuator_recovery_us) {}

Status PlantV2Application::tick(std::uint64_t now_us) {
    const Status base_status = base_.tick(now_us);
    if (!base_status.ok()) {
        return base_status;
    }
    update_sleep_sampling();
    if (lifecycle_.snapshot().state == DeviceState::Updating) {
        // OTA 期间只保留基础通信和输出安全维护。尤其是从 Fault 恢复时，不再轮询已故障
        // 的位置反馈，否则同一硬件故障会立即把 Updating 打回 Fault 并中断传输。
        return Status::success();
    }

    const BehaviorSnapshot behavior = behavior_.snapshot();
    if (behavior.state == BehaviorRunState::Idle) {
        if (lifecycle_.snapshot().state == DeviceState::Idle) {
            // 前台行为完成后释放灯光；随后由当前传感状态重新选择背景灯效。
            light_.clear(LightRequestSource::ForegroundBehavior);
        }
        const Status motion_status = poll_motion(now_us);
        if (!motion_status.ok()) {
            if (motion_status.code() == ErrorCode::Busy) {
                // 静止位置反馈正在确认瞬态异常。本 Tick 不推进传感奖励或启动舵机，
                // 下一次有效采样会恢复；连续异常则由 Adapter 升级为 MotionFailure。
                return Status::success();
            }
            return enter_fault(motion_status);
        }
    }

    bool actuator_interference = motion_.position_snapshot().moving || haptic_.active() ||
                                 behavior.state == BehaviorRunState::Running;
    if (actuator_interference) {
        const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        actuator_interference_until_us_ =
            actuator_recovery_us_ > maximum - now_us
                ? maximum
                : now_us + actuator_recovery_us_;
    } else {
        actuator_interference = now_us < actuator_interference_until_us_;
    }
    const Status sensor_status = poll_sensors(now_us, actuator_interference);
    if (!sensor_status.ok()) {
        return sensor_status;
    }
    return evaluate_growth(now_us);
}

Status PlantV2Application::handle_touch(TouchGesture gesture, std::uint64_t now_us) {
    if (growth_motion_active_) {
        return Status::failure(ErrorCode::Busy);
    }
    const Status touch_status = base_.handle_touch(gesture);
    if (!touch_status.ok()) {
        return touch_status;
    }
    if (gesture == TouchGesture::SingleTap) {
        (void)light_.request(
            LightRequestSource::Touch,
            LightPattern::TouchPulse,
            kNormalizedSensorMaximum,
            now_us,
            450ULL * 1000ULL);
        (void)growth_.submit(GrowthSource::Touch, now_us);
    }
    return touch_status;
}

Status PlantV2Application::handle_command(const Command& command) {
    if (!growth_motion_active_) {
        return base_.handle_command(command);
    }
    if (command.type == CommandType::Ping || command.type == CommandType::GetState) {
        return Status::success();
    }
    if (command.type != CommandType::StopBehavior) {
        return Status::failure(ErrorCode::Busy);
    }

    const Status stop_status = motion_.stop();
    growth_motion_active_ = false;
    growth_motion_source_ = GrowthSource::None;
    return_to_sleep_after_decay_ = false;
    light_.clear(LightRequestSource::Growth);
    const Status lifecycle_status =
        lifecycle_.apply_behavior_outcome(BehaviorOutcome::Stopped);
    return !stop_status.ok() ? stop_status : lifecycle_status;
}

Status PlantV2Application::handle_communication_connected() {
    return growth_motion_active_ ? Status::failure(ErrorCode::Busy)
                                 : base_.handle_communication_connected();
}

Status PlantV2Application::handle_communication_disconnected() {
    return base_.handle_communication_disconnected();
}

Status PlantV2Application::handle_idle_timeout() {
    return growth_motion_active_ ? Status::failure(ErrorCode::Busy)
                                 : base_.handle_idle_timeout();
}

bool PlantV2Application::growth_motion_active() const noexcept {
    return growth_motion_active_;
}

Behavior PlantV2Application::growth_motion_behavior() const noexcept {
    return growth_motion_source_ == GrowthSource::InactivityDecay
               ? Behavior::Retract
               : Behavior::Grow;
}

PositionSnapshot PlantV2Application::position_snapshot() const noexcept {
    return motion_.position_snapshot();
}

AcousticSnapshot PlantV2Application::acoustic_snapshot() const noexcept {
    return acoustic_.snapshot();
}

IlluminationSnapshot PlantV2Application::illumination_snapshot() const noexcept {
    return illumination_.snapshot();
}

ClimateSnapshot PlantV2Application::climate_snapshot() const noexcept {
    return climate_.snapshot();
}

BatterySnapshot PlantV2Application::battery_snapshot() const noexcept {
    return battery_.snapshot();
}

GrowthSnapshot PlantV2Application::growth_snapshot() const noexcept {
    return growth_.snapshot();
}

bool PlantV2Application::boot_sensors_ready() const noexcept {
    return acoustic_seen_ && illumination_seen_ && climate_seen_ && battery_seen_;
}

bool PlantV2Application::boot_critical_sensors_ok() const noexcept {
    // 舵机位置反馈参与机械闭环，是启动安全门；其余环境传感器故障按能力降级，
    // 仍通过各自快照和 BLE active_fault 上报，不能拖垮绑定、OTA 和基础交互。
    return motion_.position_snapshot().feedback == PositionFeedbackState::Valid;
}

Status PlantV2Application::poll_motion(std::uint64_t now_us) {
    MotionPollResult result{};
    const Status status = motion_.poll(now_us, result);
    if (!status.ok()) {
        return status;
    }
    if (!growth_motion_active_ || !result.completed ||
        result.execution_id != growth_execution_id_) {
        return Status::success();
    }

    growth_motion_active_ = false;
    light_.clear(LightRequestSource::Growth);
    const bool was_decay = growth_motion_source_ == GrowthSource::InactivityDecay;
    growth_motion_source_ = GrowthSource::None;
    if (!was_decay) {
        (void)light_.request(
            LightRequestSource::Growth,
            LightPattern::GrowthRise,
            kNormalizedSensorMaximum,
            now_us,
            400ULL * 1000ULL);
    }
    const Status lifecycle_status =
        lifecycle_.apply_behavior_outcome(BehaviorOutcome::CompletedIdle);
    if (!lifecycle_status.ok()) {
        return lifecycle_status;
    }
    if (was_decay && return_to_sleep_after_decay_) {
        return_to_sleep_after_decay_ = false;
        return base_.handle_idle_timeout();
    }
    return Status::success();
}

Status PlantV2Application::poll_sensors(
    std::uint64_t now_us,
    bool actuator_interference) {
    bool available = false;
    Status status{};
    const bool sleeping = lifecycle_.snapshot().state == DeviceState::Sleeping;
    if (acoustic_sampling_enabled_) {
        AcousticSample acoustic_sample{};
        status = acoustic_port_.poll(now_us, acoustic_sample, available);
        if (!status.ok()) {
            available = true;
            acoustic_sample = AcousticSample{};
        }
        if (available) {
            acoustic_seen_ = true;
            if (acoustic_.process(now_us, acoustic_sample, actuator_interference)) {
                (void)growth_.submit(GrowthSource::SustainedSpeech, now_us);
            }
            const AcousticSnapshot snapshot = acoustic_.snapshot();
            if (snapshot.state == AcousticState::Speaking ||
                snapshot.state == AcousticState::SustainedSpeech) {
                (void)light_.request(
                    LightRequestSource::Speech,
                    LightPattern::ListeningBreath,
                    snapshot.volume_level,
                    now_us,
                    150ULL * 1000ULL);
            }
        }
    }

    IlluminationSample illumination_sample{};
    available = false;
    status = illumination_port_.poll(now_us, illumination_sample, available);
    if (!status.ok()) {
        available = true;
        illumination_sample = IlluminationSample{};
    }
    if (available) {
        illumination_seen_ = true;
        const bool rgb_mask = light_.interferes_with_illumination();
        if (illumination_.process(now_us, illumination_sample, rgb_mask)) {
            (void)growth_.submit(GrowthSource::BrightExposure, now_us);
        }
        const IlluminationSnapshot snapshot = illumination_.snapshot();
        if (!sleeping && snapshot.state == IlluminationState::BrightExposure) {
            (void)light_.request(
                LightRequestSource::Sunlight,
                LightPattern::SunGlow,
                snapshot.relative_level,
                now_us,
                250ULL * 1000ULL);
        }
    }

    ClimateSample climate_sample{};
    available = false;
    status = climate_port_.poll(now_us, climate_sample, available);
    if (!status.ok()) {
        available = true;
        climate_sample = ClimateSample{};
    }
    if (available) {
        climate_seen_ = true;
        if (climate_.process(now_us, climate_sample)) {
            (void)growth_.submit(GrowthSource::SuitableClimate, now_us);
        }
        if (!sleeping && climate_.snapshot().state == ClimateState::Suitable) {
            (void)light_.request(
                LightRequestSource::Climate,
                LightPattern::ComfortGlow,
                600,
                now_us,
                2500ULL * 1000ULL);
        }
    }

    BatterySample battery_sample{};
    available = false;
    status = battery_port_.poll(now_us, battery_sample, available);
    if (!status.ok()) {
        available = true;
        battery_sample = BatterySample{};
    }
    if (available) {
        battery_seen_ = true;
        battery_.process(battery_sample);
    }
    return Status::success();
}

Status PlantV2Application::evaluate_growth(std::uint64_t now_us) {
    if (growth_motion_active_) {
        return Status::success();
    }
    const LifecycleSnapshot lifecycle = lifecycle_.snapshot();
    const bool behavior_available =
        behavior_.snapshot().state == BehaviorRunState::Idle &&
        ota_.snapshot().state == OtaState::Idle;
    const bool foreground_available =
        lifecycle.state == DeviceState::Idle && behavior_available;
    const bool inactivity_decay_available =
        (lifecycle.state == DeviceState::Idle ||
         (lifecycle.state == DeviceState::Sleeping &&
          lifecycle.power_mode == PowerMode::LightSleep)) &&
        behavior_available;
    GrowthDecision decision{};
    const Status status = growth_.evaluate(
        now_us,
        foreground_available,
        inactivity_decay_available,
        motion_.position_snapshot(),
        decision);
    if (!status.ok()) {
        return enter_fault(status);
    }
    if (decision.action == GrowthAction::None) {
        return Status::success();
    }
    if (decision.action == GrowthAction::ShowLimit) {
        return light_.request(
            LightRequestSource::Growth,
            LightPattern::GrowthLimit,
            kNormalizedSensorMaximum,
            now_us,
            500ULL * 1000ULL);
    }

    const bool decay = decision.source == GrowthSource::InactivityDecay;
    if (decay && lifecycle.state == DeviceState::Sleeping) {
        const Status wake_status = base_.wake_for_background_motion();
        if (!wake_status.ok()) {
            return wake_status;
        }
        return_to_sleep_after_decay_ = true;
    }
    const Status lifecycle_status = lifecycle_.begin_behavior(
        decay ? Behavior::Retract : Behavior::Grow);
    if (!lifecycle_status.ok()) {
        return lifecycle_status;
    }
    growth_execution_id_ = next_growth_execution_id();
    const Status motion_status = motion_.move_to(
        decision.target_position, growth_execution_id_);
    if (!motion_status.ok()) {
        return enter_fault(motion_status);
    }
    growth_motion_active_ = true;
    growth_motion_source_ = decision.source;
    if (decay) {
        light_.clear(LightRequestSource::Growth);
        return Status::success();
    }
    return light_.request(
        LightRequestSource::Growth,
        LightPattern::GrowthRise,
        kNormalizedSensorMaximum,
        now_us);
}

Status PlantV2Application::enter_fault(Status cause) {
    growth_motion_active_ = false;
    growth_motion_source_ = GrowthSource::None;
    return_to_sleep_after_decay_ = false;
    (void)motion_.stop();
    (void)base_.handle_behavior_event(
        BehaviorEvent{BehaviorEventType::FaultRaised, 0});
    return cause;
}

void PlantV2Application::update_sleep_sampling() {
    const bool should_enable = lifecycle_.snapshot().state != DeviceState::Sleeping;
    if (should_enable == acoustic_sampling_enabled_) {
        return;
    }
    acoustic_sampling_enabled_ = should_enable;
    acoustic_.set_enabled(should_enable);
    (void)acoustic_port_.set_enabled(should_enable);
    if (!should_enable) {
        // 浅睡关闭麦克风以控制功耗和隐私；光照、温湿度和电量仍低频采样，
        // 环境奖励进入最多四项的 RAM 队列，唤醒后再逐项驱动舵机和灯光。
        light_.clear(LightRequestSource::Speech);
        light_.clear(LightRequestSource::Sunlight);
        light_.clear(LightRequestSource::Climate);
    }
}

std::uint32_t PlantV2Application::next_growth_execution_id() noexcept {
    growth_execution_id_ = (growth_execution_id_ + 1U) & 0x7FFFFFFFU;
    if (growth_execution_id_ == 0) {
        growth_execution_id_ = 1;
    }
    return growth_execution_id_ | 0x80000000U;
}

}  // namespace plant
