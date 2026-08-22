#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/behavior.hpp"

namespace plant {

class IHapticPort {
public:
    virtual ~IHapticPort() = default;
    virtual Status play(HapticPattern pattern, std::uint32_t execution_id) = 0;
    virtual Status stop() = 0;
    virtual Status tick(std::uint64_t now_us) = 0;
    // 表示当前是否正在向振动执行器输出能量，供声学策略屏蔽设备自噪声。
    [[nodiscard]] virtual bool active() const noexcept = 0;
};

}  // namespace plant
