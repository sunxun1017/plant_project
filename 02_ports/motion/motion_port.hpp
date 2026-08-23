#pragma once

#include "01_core/common/status.hpp"

namespace plant {

// V2 行为服务只需要安全停止运动；生长和萎缩通过位置运动接口单独完成。
class IMotionPort {
public:
    virtual ~IMotionPort() = default;
    virtual Status stop() = 0;
};

}  // namespace plant
