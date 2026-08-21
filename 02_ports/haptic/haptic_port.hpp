#pragma once

#include "01_core/common/status.hpp"
#include "01_core/domain/behavior.hpp"

namespace plant {

class IHapticPort {
public:
    virtual ~IHapticPort() = default;
    virtual Status play(HapticPattern pattern, std::uint32_t execution_id) = 0;
    virtual Status stop() = 0;
};

}  // namespace plant
