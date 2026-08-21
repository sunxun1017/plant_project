#pragma once

#include <cstdint>

namespace plant {

enum class DeviceState : std::uint8_t {
    Booting = 0,
    Idle,
    Interacting,
    Sleeping,
    Fault,
    Updating,
};

enum class PowerMode : std::uint8_t {
    Active = 0,
    LightSleep,
    DeepSleep,
};

}  // namespace plant
