#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/sensing.hpp"

namespace plant {

class IBatterySensorPort {
public:
    virtual ~IBatterySensorPort() = default;
    virtual Status initialize() = 0;
    virtual Status poll(
        std::uint64_t now_us,
        BatterySample& sample,
        bool& available) = 0;
};

}  // namespace plant
