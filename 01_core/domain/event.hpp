#pragma once

#include <cstdint>

namespace plant {

enum class BehaviorEventType : std::uint8_t {
    MotionCompleted = 0,
    MotionFailed,
    LightingFailed,
    HapticFailed,
    BehaviorTimeout,
    StopRequested,
    FaultRaised,
};

struct BehaviorEvent {
    BehaviorEventType type;
    std::uint32_t execution_id;
};

enum class BehaviorOutcome : std::uint8_t {
    None = 0,
    CompletedIdle,
    CompletedSleeping,
    Stopped,
    Faulted,
};

enum class TouchGesture : std::uint8_t {
    SingleTap = 0,
    LongPress,
};

}  // namespace plant
