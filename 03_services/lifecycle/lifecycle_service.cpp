#include "03_services/lifecycle/lifecycle_service.hpp"

namespace plant {

LifecycleSnapshot LifecycleService::snapshot() const noexcept {
    return LifecycleSnapshot{state_, power_mode_};  // 返回调用时的生命周期和功耗模式快照。
}

Status LifecycleService::finish_boot(bool self_test_passed) noexcept {
    if (state_ != DeviceState::Booting) {   // 判断上一个状态是否是booting
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
    if (state_ != DeviceState::Idle && state_ != DeviceState::Sleeping &&
        state_ != DeviceState::Fault) {
        return Status::failure(ErrorCode::InvalidState);
    }
    // Fault 已满足输出进入安全状态的生命周期不变量。恢复性 OTA 可以直接开始；若升级
    // 取消或失败，必须回到 Fault，不能把未修复的设备误报为 Idle。
    update_return_state_ = state_ == DeviceState::Fault ? DeviceState::Fault
                                                        : DeviceState::Idle;
    state_ = DeviceState::Updating;
    power_mode_ = PowerMode::Active;
    return Status::success();
}

Status LifecycleService::cancel_update() noexcept {
    if (state_ != DeviceState::Updating) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = update_return_state_;
    update_return_state_ = DeviceState::Idle;
    power_mode_ = PowerMode::Active;
    return Status::success();
}

Status LifecycleService::finish_update_and_reboot() noexcept {
    if (state_ != DeviceState::Updating) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = DeviceState::Booting;
    power_mode_ = PowerMode::Active;
    update_return_state_ = DeviceState::Idle;
    return Status::success();
}

Status LifecycleService::fail_update(bool recoverable) noexcept {
    if (state_ != DeviceState::Updating) {
        return Status::failure(ErrorCode::InvalidState);
    }
    state_ = recoverable ? update_return_state_ : DeviceState::Fault;
    power_mode_ = PowerMode::Active;
    update_return_state_ = DeviceState::Idle;
    return Status::failure(ErrorCode::OtaFailure);
}

Status LifecycleService::enter_light_sleep_mode() noexcept {
    if (state_ != DeviceState::Sleeping) {
        return Status::failure(ErrorCode::InvalidState);
    }
    power_mode_ = PowerMode::LightSleep;
    return Status::success();
}

Status LifecycleService::enter_deep_sleep_mode() noexcept {
    if (state_ != DeviceState::Sleeping) {
        return Status::failure(ErrorCode::InvalidState);
    }
    power_mode_ = PowerMode::DeepSleep;
    return Status::success();
}

Status LifecycleService::restore_active_power_mode() noexcept {
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
