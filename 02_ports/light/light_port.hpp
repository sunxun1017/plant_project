#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/behavior.hpp"

namespace plant {

class ILightPort {
public:
    virtual ~ILightPort() = default;
    virtual Status play(LightPattern pattern, std::uint32_t execution_id) = 0;
    virtual Status stop() = 0;
    virtual Status tick(std::uint64_t now_us) = 0;
};

}  // namespace plant
