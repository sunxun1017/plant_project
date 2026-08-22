#pragma once

#include <cstdint>

namespace plant {

enum class Behavior : std::uint8_t {
    WakeUp = 0,
    Happy,
    Attention,
    Calm,
    Sleep,
    Error,
    Grow,
    Retract,
};

enum class MotionPattern : std::uint8_t {
    Wake = 0,
    GentleSway,
    LookUp,
    ReturnNeutral,
    MoveToSleepPose,
    StopAndHoldSafe,
};

enum class LightPattern : std::uint8_t {
    FadeIn = 0,
    SoftBreathing,
    ShortPulse,
    SlowBreathing,
    FadeOut,
    ErrorBlink,
    TouchPulse,
    ListeningBreath,
    SunGlow,
    ComfortGlow,
    GrowthRise,
    GrowthLimit,
};

enum class HapticPattern : std::uint8_t {
    Off = 0,
    SoftPulse,
    DoubleSoftPulse,
    Warning,
};

enum class CompletionTarget : std::uint8_t {
    Idle = 0,
    Sleeping,
    Fault,
};

enum class InterruptionReason : std::uint8_t {
    NormalCommand = 0,
    Touch,
    Stop,
    WakeSleep,
    Fault,
};

struct BehaviorPlan {
    Behavior behavior;
    MotionPattern motion;
    LightPattern lighting;
    HapticPattern haptic;
    CompletionTarget completion;
    bool interruptible_by_normal_event;
};

}  // namespace plant
