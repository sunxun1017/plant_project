#pragma once

#include <cstdint>

#include "01_core/domain/sensing.hpp"
#include "02_ports/motion/motion_port.hpp"

namespace plant {

// V2 带反馈运动边界。目标和反馈都使用 0..1000 的机构归一化位置，
// 不暴露 ADC 原始码，也不以最后一次 PWM 命令代替真实位置。
class IPositionMotionPort : public IMotionPort {
public:
    ~IPositionMotionPort() override = default;
    virtual Status move_to(std::uint16_t target_position, std::uint32_t execution_id) = 0;
    [[nodiscard]] virtual PositionSnapshot position_snapshot() const noexcept = 0;
};

}  // namespace plant
