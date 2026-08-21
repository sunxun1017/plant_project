#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/event.hpp"

namespace plant {

class ITouchPort {
public:
    virtual ~ITouchPort() = default;
    virtual Status initialize() = 0;
    virtual bool poll(std::uint64_t now_ms, TouchGesture& gesture) = 0;
};

}  // namespace plant
