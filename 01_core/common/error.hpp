#pragma once

#include <cstdint>

namespace plant {

enum class ErrorCode : std::uint8_t {
    None = 0,
    Busy,
    InvalidArgument,
    InvalidState,
    Unsupported,
    Timeout,
    MotionFailure,
    LightingFailure,
    HapticFailure,
    StorageFailure,
    ProtocolFailure,
    OtaFailure,
    InternalFailure,
};

}  // namespace plant
