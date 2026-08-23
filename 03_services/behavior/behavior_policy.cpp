#include "03_services/behavior/behavior_policy.hpp"

namespace plant {
namespace {

constexpr BehaviorPlan kWakeUp{
    Behavior::WakeUp,
    LightPattern::FadeIn,
    HapticPattern::SoftPulse,
    CompletionTarget::Idle,
    false,
};

constexpr BehaviorPlan kHappy{
    Behavior::Happy,
    LightPattern::SoftBreathing,
    HapticPattern::DoubleSoftPulse,
    CompletionTarget::Idle,
    true,
};

constexpr BehaviorPlan kAttention{
    Behavior::Attention,
    LightPattern::ShortPulse,
    HapticPattern::SoftPulse,
    CompletionTarget::Idle,
    true,
};

constexpr BehaviorPlan kCalm{
    Behavior::Calm,
    LightPattern::SlowBreathing,
    HapticPattern::Off,
    CompletionTarget::Idle,
    true,
};

constexpr BehaviorPlan kSleep{
    Behavior::Sleep,
    LightPattern::FadeOut,
    HapticPattern::SoftPulse,
    CompletionTarget::Sleeping,
    false,
};

constexpr BehaviorPlan kError{
    Behavior::Error,
    LightPattern::ErrorBlink,
    HapticPattern::Warning,
    CompletionTarget::Fault,
    false,
};

}  // namespace

/**
 * @brief 
 * 
 * @param behavior 根据我想让机器人进入什么状态 查看执行方案
 * @param plan 
 * @return true 
 * @return false 
 */
bool BehaviorPolicy::try_get_plan(Behavior behavior, BehaviorPlan& plan) noexcept {
    switch (behavior) {
        case Behavior::WakeUp:
            plan = kWakeUp;
            return true;
        case Behavior::Happy:
            plan = kHappy;
            return true;
        case Behavior::Attention:
            plan = kAttention;
            return true;
        case Behavior::Calm:
            plan = kCalm;
            return true;
        case Behavior::Sleep:
            plan = kSleep;
            return true;
        case Behavior::Error:
            plan = kError;
            return true;
        case Behavior::Grow:
        case Behavior::Retract:
            // Grow/Retract 由 V2 Growth Service 基于实测位置生成绝对目标，不使用固定计划。
            return false;
    }
    return false;
}

bool BehaviorPolicy::can_interrupt(
    const BehaviorPlan& current,
    Behavior incoming,
    InterruptionReason reason) noexcept {
    if (incoming == current.behavior) {
        return false;
    }
    if (reason == InterruptionReason::Fault || reason == InterruptionReason::Stop) {
        return true;
    }
    if (current.behavior == Behavior::Error) {
        return false;
    }
    if (reason == InterruptionReason::WakeSleep) {
        return incoming == Behavior::WakeUp || incoming == Behavior::Sleep;
    }
    return current.interruptible_by_normal_event;
}

}  // namespace plant
