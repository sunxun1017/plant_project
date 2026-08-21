#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/behavior.hpp"

namespace plant {

struct MotionPollResult {
    bool completed{false};
    std::uint32_t execution_id{0};
};

class IMotionPort {
public:
    virtual ~IMotionPort() = default;
    virtual Status play(MotionPattern pattern, std::uint32_t execution_id) = 0;
    virtual Status stop() = 0;
    virtual Status poll(std::uint64_t now_us, MotionPollResult& result) = 0;
};

}  // namespace plant
