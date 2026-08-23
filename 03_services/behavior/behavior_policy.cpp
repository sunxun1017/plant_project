/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 16:48:46
 * @FilePath: /plant_project/03_services/behavior/behavior_policy.cpp
 * @Description:
 */
#include "03_services/behavior/behavior_policy.hpp"

namespace plant {
namespace {

constexpr BehaviorPlan kWakeUp{
    Behavior::WakeUp,
    LightCue::WakeUp,
    HapticPattern::SoftPulse,
    CompletionTarget::Idle,
    false,
};

constexpr BehaviorPlan kHappy{
    Behavior::Happy,
    LightCue::Happy,
    HapticPattern::DoubleSoftPulse,
    CompletionTarget::Idle,
    true,
};

constexpr BehaviorPlan kSleep{
    Behavior::Sleep,
    LightCue::Sleep,
    HapticPattern::SoftPulse,
    CompletionTarget::Sleeping,
    false,
};

constexpr BehaviorPlan kError{
    Behavior::Error,
    LightCue::Error,
    HapticPattern::Warning,
    CompletionTarget::Fault,
    false,
};

}  // namespace

bool BehaviorPolicy::try_get_plan(Behavior behavior, BehaviorPlan& plan) noexcept {
    switch (behavior) {
        case Behavior::WakeUp:
            plan = kWakeUp;
            return true;
        case Behavior::Happy:
            plan = kHappy;
            return true;
        case Behavior::Sleep:
            plan = kSleep;
            return true;
        case Behavior::Error:
            plan = kError;
            return true;
        case Behavior::Grow:
        case Behavior::Retract:
            // Grow/Retract 由 GrowthService 基于实测位置生成目标，不使用固定行为表现。
            return false;
        case Behavior::None:
            return false;
    }
    return false;
}

bool BehaviorPolicy::can_interrupt(
    const BehaviorPlan& current,
    Behavior incoming,
    InterruptionReason reason) noexcept {
    if (incoming == current.behavior) { // 新行为等于当前行为 就不可以打断
        return false;   // TODO： 它这个意思难道是连续两次相同的行为 它会不打断当前行为 保证一致
    }
    if (reason == InterruptionReason::Fault || reason == InterruptionReason::Stop) {    // 故障这种比较严重的 肯定会打断的
        return true;
    }
    if (current.behavior == Behavior::Error) {  // 连着两次都是false也不会打断的
        return false;
    }
    if (reason == InterruptionReason::WakeSleep) {  // 睡眠切换打断时 并且行为是Wakeup 或者是 Sleep可以打断 同时顺序保证了连续两次不会打断 也就是睡眠不会打断睡眠
        return incoming == Behavior::WakeUp || incoming == Behavior::Sleep;
    }
    return current.interruptible_by_normal_event;
}

}  // namespace plant
