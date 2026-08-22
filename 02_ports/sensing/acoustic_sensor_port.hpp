#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/sensing.hpp"

namespace plant {

// 只暴露低频包络的相对音量；该边界不允许传递原始或可重建音频样本。
class IAcousticSensorPort {
public:
    virtual ~IAcousticSensorPort() = default;
    virtual Status initialize() = 0;
    virtual Status set_enabled(bool enabled) = 0;
    virtual Status poll(
        std::uint64_t now_us,
        AcousticSample& sample,
        bool& available) = 0;
};

}  // namespace plant
