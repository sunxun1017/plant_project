#include "03_services/lifecycle/lifecycle_service.hpp"

namespace plant {

LifecycleSnapshot LifecycleService::snapshot() const noexcept {
    return LifecycleSnapshot{state_, power_mode_};  // 返回调用时的生命周期和功耗模式快照。
}

Status LifecycleService::finish_boot(bool self_test_passed) noexcept {
    if (state_ != DeviceState::Booting) {
        return Status::failure(ErrorCode::InvalidState);
    }
    power_mode_ = PowerMode::Active;
    state_ = self_test_passed ? DeviceState::Idle : DeviceState::Fault;
    return Status::success();
}

Status LifecycleService::begin_behavior(Behavior behavior) noexcept {
    if (state_ == DeviceState::Updating || state_ == DeviceState::Fault ||
        state_ == DeviceState::Booting) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (state_ == DeviceState::Sleeping && behavior != Behavior::WakeUp) {
        return Status::failure(ErrorCode::InvalidState);
    }
    power_mode_ = PowerMode::Active;
    state_ = behavior == Behavior::Error ? DeviceState::Fault : DeviceState::Interacting;
    return Status::success();
}

Status LifecycleService::apply_behavior_outcome(BehaviorOutcome outcome) noexcept {
    if (state_ != DeviceState::Interacting && outcome != BehaviorOutcome::Faulted) {
        return Status::failure(ErrorCode::InvalidState);
    }
    switch (outcome) {
        case BehaviorOutcome::CompletedIdle:
        case BehaviorOutcome::Stopped:
            state_ = DeviceState::Idle;
            power_mode_ = PowerMode::Active;
            return Status::success();
        case BehaviorOutcome::CompletedSleeping:
            state_ = DeviceState::Sleeping;
            power_mode_ = PowerMode::Active;
            return Status::success();
        case BehaviorOutcome::Faulted:
            state_ = DeviceState::Fault;
            power_mode_ = PowerMode::Active;
            return Status::success();
        case BehaviorOutcome::None:
            return Status::failure(ErrorCode::InvalidArgument);
    }
    return Status::failure(ErrorCode::InternalFailure);
}

Status LifecycleService::begin_update() noexcept {
    if (state_ != DeviceState::Idle && state_ != DeviceState::Sleeping) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = DeviceState::Updating;
    power_mode_ = PowerMode::Active;
    return Status::success();
}

Status LifecycleService::cancel_update() noexcept {
    if (state_ != DeviceState::Updating) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = DeviceState::Idle;
    return Status::success();
}

Status LifecycleService::finish_update_and_reboot() noexcept {
    if (state_ != DeviceState::Updating) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = DeviceState::Booting;
    power_mode_ = PowerMode::Active;
    return Status::success();
}

Status LifecycleService::fail_update(bool recoverable) noexcept {
    if (state_ != DeviceState::Updating) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = recoverable ? DeviceState::Idle : DeviceState::Fault;
    power_mode_ = PowerMode::Active;
    return Status::failure(ErrorCode::OtaFailure);
}

Status LifecycleService::enter_light_sleep() noexcept {
    if (state_ != DeviceState::Sleeping) {
        return Status::failure(ErrorCode::InvalidState);
    }
    power_mode_ = PowerMode::LightSleep;
    return Status::success();
}

Status LifecycleService::enter_deep_sleep() noexcept {
    if (state_ != DeviceState::Sleeping) {
        return Status::failure(ErrorCode::InvalidState);
    }
    power_mode_ = PowerMode::DeepSleep;
    return Status::success();
}

Status LifecycleService::leave_low_power() noexcept {
    if (state_ != DeviceState::Sleeping) {
        return Status::failure(ErrorCode::InvalidState);
    }
    power_mode_ = PowerMode::Active;
    return Status::success();
}

Status LifecycleService::wake(bool requires_reinitialization) noexcept {
    if (state_ != DeviceState::Sleeping) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = requires_reinitialization ? DeviceState::Booting : DeviceState::Idle;
    power_mode_ = PowerMode::Active;
    return Status::success();
}

Status LifecycleService::raise_fault() noexcept {
    state_ = DeviceState::Fault;
    power_mode_ = PowerMode::Active;
    return Status::success();
}

Status LifecycleService::reset_fault() noexcept {
    if (state_ != DeviceState::Fault) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = DeviceState::Booting;
    power_mode_ = PowerMode::Active;
    return Status::success();
}

}  // namespace plant
