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

    if (state_ == BehaviorRunState::Fault && behavior != Behavior::Error) {
        return Status::failure(ErrorCode::InvalidState);
    }

    if (state_ == BehaviorRunState::Running) {
        if (behavior == current_behavior_) {
            return Status::success();
        }
        if (!BehaviorPolicy::can_interrupt(current_plan_, behavior, reason)) {
            return Status::failure(ErrorCode::Busy);
        }
        stop_outputs();
    }

    return start_plan(plan);
}

Status BehaviorService::start_plan(const BehaviorPlan& plan) {
    current_plan_ = plan;
    current_behavior_ = plan.behavior;
    execution_id_ = next_execution_id();
    outcome_ = BehaviorOutcome::None;
    start_time_initialized_ = false;

    Status status = config_.expressive_motion_enabled
                        ? motion_.play(plan.motion, execution_id_)
                        : motion_.stop();
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

    if (plan.completion == CompletionTarget::Fault) {
        state_ = BehaviorRunState::Fault;
        outcome_ = BehaviorOutcome::Faulted;
    } else {
        state_ = BehaviorRunState::Running;
        if (!config_.expressive_motion_enabled) {
            // 仅生长产品的普通表现没有机械阶段；灯光和振动仍由各自非阻塞动画推进，
            // 生命周期无需等待一个不存在的舵机完成事件。
            return complete_current();
        }
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

    MotionPollResult motion_result{};
    if (state_ == BehaviorRunState::Running) {
        // 运动完成事件只由当前行为消费。V2 在行为空闲时可能由 Growth Service
        // 独占同一运动端口，Behavior Service 不得抢走它的完成事件。
        const Status motion_status = motion_.poll(now_us, motion_result);
        if (!motion_status.ok()) {
            return enter_fault_with(motion_status.code());
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

    if (motion_result.completed && state_ == BehaviorRunState::Running) {
        return handle_event(
            BehaviorEvent{BehaviorEventType::MotionCompleted, motion_result.execution_id});
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
    (void)motion_.play(MotionPattern::StopAndHoldSafe, fault_id);
    (void)light_.play(LightPattern::ErrorBlink, fault_id);
    (void)haptic_.play(HapticPattern::Warning, fault_id);
}

Status BehaviorService::complete_current() {
    if (current_behavior_ == Behavior::WakeUp) {
        const Status light_status = light_.play(LightPattern::SoftBreathing, execution_id_);
        if (!light_status.ok()) {
            return enter_fault_with(light_status.code());
        }
    }
    state_ = BehaviorRunState::Idle;
    start_time_initialized_ = false;
    if (current_plan_.completion == CompletionTarget::Sleeping) {
        outcome_ = BehaviorOutcome::CompletedSleeping;
    } else {
        outcome_ = BehaviorOutcome::CompletedIdle;
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
