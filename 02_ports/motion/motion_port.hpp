#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/behavior.hpp"

namespace plant {

struct MotionPollResult {
    bool completed{false};
    std::uint32_t execution_id{0};
};

// 平台无关的运动能力边界：I 表示接口，Port 表示架构边界；这里只定义能力，
// 具体的舵机、步进电机或 Fake 实现在 Adapter 中完成。
class IMotionPort {
public:
    virtual ~IMotionPort() = default;
    virtual Status play(MotionPattern pattern, std::uint32_t execution_id) = 0;
    virtual Status stop() = 0;
    virtual Status poll(std::uint64_t now_us, MotionPollResult& result) = 0;
};

}  // namespace plant
