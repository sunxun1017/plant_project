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
    // V2 动态灯效使用 0..1000 的低频强度；V1 Adapter 可忽略并返回 Unsupported。
    virtual Status set_intensity(std::uint16_t) {
        return Status::failure(ErrorCode::Unsupported);
    }
};

}  // namespace plant
