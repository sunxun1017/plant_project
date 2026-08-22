#include "07_products/plant_v1/app/plant_application.hpp"

namespace plant {

PlantApplication::PlantApplication(
    BehaviorService& behavior,
    LifecycleService& lifecycle,
    PowerService& power,
    OtaService& ota) noexcept
    : behavior_(behavior), lifecycle_(lifecycle), power_(power), ota_(ota) {}

Status PlantApplication::finish_boot(bool self_test_passed) {
    return lifecycle_.finish_boot(self_test_passed);
}

Status PlantApplication::tick(std::uint64_t now_us) {
    return apply_pending_behavior_outcome(behavior_.tick(now_us));
}

Status PlantApplication::handle_touch(TouchGesture gesture) {
    const DeviceState state = lifecycle_.snapshot().state;
    if (state == DeviceState::Booting || state == DeviceState::Fault ||
        state == DeviceState::Updating) {
        return Status::failure(ErrorCode::InvalidState);
    }

    if (gesture == TouchGesture::LongPress) {
        return start_behavior(
            state == DeviceState::Sleeping ? Behavior::WakeUp : Behavior::Sleep,
            InterruptionReason::WakeSleep);
    }
    return start_behavior(
        state == DeviceState::Sleeping ? Behavior::WakeUp : Behavior::Happy,
        InterruptionReason::Touch);
}

Status PlantApplication::handle_communication_connected() {
    const DeviceState state = lifecycle_.snapshot().state;
    if (state == DeviceState::Sleeping) {
        return request_behavior(Behavior::WakeUp, InterruptionReason::WakeSleep);
    }
    if (state == DeviceState::Idle || state == DeviceState::Interacting) {
        return request_behavior(Behavior::Attention, InterruptionReason::NormalCommand);
    }
    return Status::failure(ErrorCode::InvalidState);
}

Status PlantApplication::handle_communication_disconnected() {
    const OtaState ota_state = ota_.snapshot().state;
    if (!ota_pending_ && ota_state != OtaState::Receiving &&
        ota_state != OtaState::Verifying) {
        return Status::success();
    }
    return cancel_ota();
}

Status PlantApplication::request_behavior(Behavior behavior, InterruptionReason reason) {
    const DeviceState state = lifecycle_.snapshot().state;
    if ((behavior == Behavior::WakeUp && state == DeviceState::Idle) ||
        (behavior == Behavior::Sleep && state == DeviceState::Sleeping)) {
        return Status::success();
    }
    return start_behavior(behavior, reason);
}

Status PlantApplication::stop_behavior() {
    const DeviceState state = lifecycle_.snapshot().state;
    if (state == DeviceState::Idle) {
        return Status::success();
    }
    if (state != DeviceState::Interacting) {
        return Status::failure(ErrorCode::InvalidState);
    }
    const Status status = behavior_.stop();
    if (!status.ok()) {
        return status;
    }
    return apply_behavior_outcome(behavior_.take_outcome());
}

Status PlantApplication::handle_behavior_event(const BehaviorEvent& event) {
    return apply_pending_behavior_outcome(behavior_.handle_event(event));
}

Status PlantApplication::apply_pending_behavior_outcome(Status operation_status) {
    const BehaviorOutcome outcome = behavior_.take_outcome();
    if (outcome != BehaviorOutcome::None) {
        const Status outcome_status = apply_behavior_outcome(outcome);
        if (!outcome_status.ok()) {
            return outcome_status;
        }
    }
    return operation_status;
}

Status PlantApplication::handle_idle_timeout() {
    if (lifecycle_.snapshot().state != DeviceState::Idle) {
        return Status::failure(ErrorCode::InvalidState);
    }
    // WakeSleep 表示睡眠/唤醒类高优先级转换原因，不是“当前处于唤醒状态”。
    return start_behavior(Behavior::Sleep, InterruptionReason::WakeSleep);
}

Status PlantApplication::handle_command(const Command& command) {
    switch (command.type) {
        case CommandType::Ping:
        case CommandType::GetState:
            return Status::success();
        case CommandType::SetBehavior: {
            const InterruptionReason reason =
                command.behavior == Behavior::WakeUp || command.behavior == Behavior::Sleep
                    ? InterruptionReason::WakeSleep
                    : InterruptionReason::NormalCommand;
            return request_behavior(command.behavior, reason);
        }
        case CommandType::StopBehavior:
            return stop_behavior();
        case CommandType::BeginOta:
            return begin_ota(command.ota_metadata);
        case CommandType::OtaChunk:
            return write_ota_chunk(
                command.ota_offset, command.ota_data.data(), command.ota_data_size);
        case CommandType::FinishOta:
            return finish_ota();
        case CommandType::CancelOta:
            return cancel_ota();
    }
    return Status::failure(ErrorCode::Unsupported);
}

Status PlantApplication::begin_ota(const OtaImageMetadata& metadata) {
    const Status validation = ota_.validate(metadata);
    if (!validation.ok()) {
        return validation;
    }

    const LifecycleSnapshot lifecycle = lifecycle_.snapshot();
    if (lifecycle.state == DeviceState::Sleeping) {
        if (lifecycle.power_mode == PowerMode::LightSleep) {
            const Status wake_status = power_.handle_wake();
            if (!wake_status.ok()) {
                return wake_status;
            }
        }
        return ota_.begin(metadata);
    }
    if (lifecycle.state != DeviceState::Idle &&
        lifecycle.state != DeviceState::Interacting) {
        return Status::failure(ErrorCode::InvalidState);
    }

    pending_ota_ = metadata;
    ota_pending_ = true;
    const Status status = start_behavior(Behavior::Sleep, InterruptionReason::WakeSleep);
    if (!status.ok()) {
        ota_pending_ = false;
    }
    return status;
}

Status PlantApplication::write_ota_chunk(
    std::size_t offset,
    const std::uint8_t* data,
    std::size_t size) {
    return ota_.write_chunk(offset, data, size);
}

Status PlantApplication::finish_ota() {
    return ota_.finish();
}

Status PlantApplication::cancel_ota() {
    if (ota_pending_) {
        ota_pending_ = false;
        return Status::success();
    }
    ota_pending_ = false;
    return ota_.cancel();
}

LifecycleSnapshot PlantApplication::lifecycle_snapshot() const noexcept {
    return lifecycle_.snapshot();
}

BehaviorSnapshot PlantApplication::behavior_snapshot() const noexcept {
    return behavior_.snapshot();
}

OtaSnapshot PlantApplication::ota_snapshot() const noexcept {
    return ota_.snapshot();
}

Status PlantApplication::start_behavior(Behavior behavior, InterruptionReason reason) { // 所有非阻塞语义行为的统一启动入口。
    const LifecycleSnapshot before = lifecycle_.snapshot(); // 保存启动行为前的生命周期和功耗模式。
    if (before.state == DeviceState::Sleeping &&
        before.power_mode == PowerMode::LightSleep && behavior == Behavior::WakeUp) {
        const Status wake_status = power_.handle_wake(); // 先退出轻睡眠，再启动 WakeUp 表现。
        if (!wake_status.ok()) {
            return wake_status;
        }
    }
    // begin_behavior() 会先把普通行为的生命周期切换为 Interacting。
    const Status lifecycle_status = lifecycle_.begin_behavior(behavior);
    if (!lifecycle_status.ok()) {
        return lifecycle_status;
    }

    // TODO: behavior_.start() 若以 Busy/Unsupported 等无 outcome 错误返回，当前代码不会
    // 恢复 before，生命周期可能停留在 Interacting；需要为这种失败补偿状态转换。
    const Status behavior_status = behavior_.start(behavior, reason); // 相同行为正在运行时按幂等成功处理。
    if (!behavior_status.ok()) { // 执行器启动失败会产生 Faulted outcome，并在这里同步生命周期。
        const BehaviorOutcome outcome = behavior_.take_outcome();
        if (outcome != BehaviorOutcome::None) {
            (void)apply_behavior_outcome(outcome);
        }
    }
    return behavior_status;
}

Status PlantApplication::apply_behavior_outcome(BehaviorOutcome outcome) {
    Status status = lifecycle_.apply_behavior_outcome(outcome);
    if (!status.ok()) {
        return status;
    }

    if (outcome != BehaviorOutcome::CompletedSleeping) {
        return Status::success();
    }
    if (ota_pending_) {
        ota_pending_ = false;
        status = ota_.begin(pending_ota_);
        if (!status.ok()) {
            (void)power_.request_light_sleep();
        }
        return status;
    }
    return power_.request_light_sleep();
}

}  // namespace plant
