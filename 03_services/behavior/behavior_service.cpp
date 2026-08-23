#include "03_services/behavior/behavior_service.hpp"

#include "03_services/behavior/behavior_policy.hpp"

namespace plant {

BehaviorService::BehaviorService(
    IMotionPort& motion,
    ILightPort& light,
    IHapticPort& haptic,
    BehaviorExecutionConfig config) noexcept
    : motion_(motion), light_(light), haptic_(haptic), config_(config) {}

Status BehaviorService::start(Behavior behavior, InterruptionReason reason) {
    BehaviorPlan plan{};
    if (!BehaviorPolicy::try_get_plan(behavior, plan)) {
        return Status::failure(ErrorCode::Unsupported);
    }

    if (state_ == BehaviorRunState::Fault && behavior != Behavior::Error) { // 如果当前已经有故障 并且下一个行为不是故障 那么就直接推出吧 
        return Status::failure(ErrorCode::InvalidState);
    }

    if (state_ == BehaviorRunState::Running) {  // 如果是正常 
        if (behavior == current_behavior_) {
            return Status::success();   // 上层行为不变 不做任何处理
        }
        if (!BehaviorPolicy::can_interrupt(current_plan_, behavior, reason)) {  // 判断是否可以打断
            return Status::failure(ErrorCode::Busy);    // 直接返回比较忙
        }
        stop_non_motion_outputs(); // 停止当前的灯光和振动
    }

    return start_plan(plan);    // 执行新的
}

Status BehaviorService::start_plan(const BehaviorPlan& plan) {
    current_plan_ = plan;
    current_behavior_ = plan.behavior;
    execution_id_ = next_execution_id();
    outcome_ = BehaviorOutcome::None;
    start_time_initialized_ = false;

    Status status = motion_.stop();// TODO： 不懂为什么这样分开
    if (!status.ok()) {
        enter_fault();
        return status;
    }
    status = light_.play(plan.lighting, execution_id_);
    if (!status.ok()) {
        enter_fault();
        return status;
    }
    status = haptic_.play(plan.haptic, execution_id_);
    if (!status.ok()) {
        enter_fault();
        return status;
    }

    if (plan.completion == CompletionTarget::Fault) {   // 如果计划的完成目标是fault设备进入故障状态
        state_ = BehaviorRunState::Fault;
        outcome_ = BehaviorOutcome::Faulted;
    } else {
        state_ = BehaviorRunState::Running; // 否则还是正常
        // V2 的普通表现没有机械阶段；灯光和振动仍由各自非阻塞动画推进，
        // 生命周期无需等待一个不存在的舵机完成事件。
        return complete_current();
    }
    return Status::success();
}

Status BehaviorService::stop() {
    if (state_ == BehaviorRunState::Fault) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (state_ == BehaviorRunState::Idle) {
        return Status::success();
    }
    stop_outputs();
    state_ = BehaviorRunState::Idle;
    outcome_ = BehaviorOutcome::Stopped;
    start_time_initialized_ = false;
    return Status::success();
}

Status BehaviorService::tick(std::uint64_t now_us) {
    if (state_ == BehaviorRunState::Running) {
        if (!start_time_initialized_) {
            started_us_ = now_us;
            start_time_initialized_ = true;
        } else if (config_.timeout_us != 0 && now_us - started_us_ >= config_.timeout_us) {
            return enter_fault_with(ErrorCode::Timeout);
        }
    }

    const Status light_status = light_.tick(now_us);
    if (!light_status.ok()) {
        return enter_fault_with(light_status.code());
    }

    const Status haptic_status = haptic_.tick(now_us);
    if (!haptic_status.ok()) {
        return enter_fault_with(haptic_status.code());
    }

    return Status::success();
}

Status BehaviorService::handle_event(const BehaviorEvent& event) {
    if (event.execution_id != 0 && event.execution_id != execution_id_) {
        return Status::success();
    }
    if (event.type == BehaviorEventType::FaultRaised) {
        enter_fault();
        return Status::failure(ErrorCode::InternalFailure);
    }
    if (state_ != BehaviorRunState::Running) {
        return Status::failure(ErrorCode::InvalidState);
    }

    switch (event.type) {
        case BehaviorEventType::MotionCompleted:
            return complete_current();
        case BehaviorEventType::StopRequested:
            return stop();
        case BehaviorEventType::MotionFailed:
            enter_fault();
            return Status::failure(ErrorCode::MotionFailure);
        case BehaviorEventType::LightingFailed:
            enter_fault();
            return Status::failure(ErrorCode::LightingFailure);
        case BehaviorEventType::HapticFailed:
            enter_fault();
            return Status::failure(ErrorCode::HapticFailure);
        case BehaviorEventType::BehaviorTimeout:
            enter_fault();
            return Status::failure(ErrorCode::Timeout);
        case BehaviorEventType::FaultRaised:
            break;
    }
    return Status::failure(ErrorCode::InternalFailure);
}

Status BehaviorService::enter_fault_with(ErrorCode error) noexcept {
    if (state_ != BehaviorRunState::Fault) {
        enter_fault();
    }
    return Status::failure(error);
}

BehaviorSnapshot BehaviorService::snapshot() const noexcept {
    return BehaviorSnapshot{state_, current_behavior_, execution_id_};
}

BehaviorOutcome BehaviorService::take_outcome() noexcept {
    const BehaviorOutcome result = outcome_;
    outcome_ = BehaviorOutcome::None;
    return result;
}

void BehaviorService::stop_outputs() noexcept {
    (void)motion_.stop();
    stop_non_motion_outputs();
}

void BehaviorService::stop_non_motion_outputs() noexcept {
    (void)light_.stop();
    (void)haptic_.stop();
}

void BehaviorService::enter_fault() noexcept {
    stop_outputs();
    state_ = BehaviorRunState::Fault;
    start_time_initialized_ = false;
    current_behavior_ = Behavior::Error;
    outcome_ = BehaviorOutcome::Faulted;
    const std::uint32_t fault_id = next_execution_id();
    (void)light_.play(LightCue::Error, fault_id);
    (void)haptic_.play(HapticPattern::Warning, fault_id);
}

Status BehaviorService::complete_current() {
    state_ = BehaviorRunState::Idle;    // 变成空闲状态
    start_time_initialized_ = false;    // 清除超时计数
    if (current_plan_.completion == CompletionTarget::Sleeping) {
        outcome_ = BehaviorOutcome::CompletedSleeping;  // 睡觉成功进入睡眠结果
    } else {
        outcome_ = BehaviorOutcome::CompletedIdle;      // 结果进入空闲
    }
    return Status::success();
}

std::uint32_t BehaviorService::next_execution_id() noexcept {
    ++execution_id_;
    if (execution_id_ == 0) {
        ++execution_id_;
    }
    return execution_id_;
}

}  // namespace plant
