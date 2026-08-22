#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/sensing.hpp"

namespace plant {

// Adapter 在内部以非阻塞状态机触发和读取 AHT21；poll() 不得等待一次完整测量。
class IClimateSensorPort {
public:
    virtual ~IClimateSensorPort() = default;
    virtual Status initialize() = 0;
    virtual Status set_enabled(bool enabled) = 0;
    virtual Status poll(
        std::uint64_t now_us,
        ClimateSample& sample,
        bool& available) = 0;
};

}  // namespace plant
