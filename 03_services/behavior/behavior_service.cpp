#include "03_services/behavior/behavior_service.hpp"

#include "03_services/behavior/behavior_policy.hpp"

namespace plant {

BehaviorService::BehaviorService(
    IMotionPort& motion,
    ILightPort& light,
    IHapticPort& haptic) noexcept
    : motion_(motion), light_(light), haptic_(haptic) {}

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

    Status status = motion_.play(plan.motion, execution_id_);
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
            complete_current();
            return Status::success();
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
    current_behavior_ = Behavior::Error;
    outcome_ = BehaviorOutcome::Faulted;
    const std::uint32_t fault_id = next_execution_id();
    (void)motion_.play(MotionPattern::StopAndHoldSafe, fault_id);
    (void)light_.play(LightPattern::ErrorBlink, fault_id);
    (void)haptic_.play(HapticPattern::Warning, fault_id);
}

void BehaviorService::complete_current() noexcept {
    state_ = BehaviorRunState::Idle;
    if (current_plan_.completion == CompletionTarget::Sleeping) {
        outcome_ = BehaviorOutcome::CompletedSleeping;
    } else {
        outcome_ = BehaviorOutcome::CompletedIdle;
    }
}

std::uint32_t BehaviorService::next_execution_id() noexcept {
    ++execution_id_;
    if (execution_id_ == 0) {
        ++execution_id_;
    }
    return execution_id_;
}

}  // namespace plant
