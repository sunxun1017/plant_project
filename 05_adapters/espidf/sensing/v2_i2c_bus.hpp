#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "driver/i2c_master.h"

namespace plant {

// V2 的 AHT21 和电量计共享同一个 I2C master bus，由 composition root 唯一持有。
class V2I2cBus final {
public:
    Status initialize();
    Status add_device(
        std::uint8_t address,
        std::uint32_t frequency_hz,
        i2c_master_dev_handle_t& device) const;

private:
    i2c_master_bus_handle_t bus_{nullptr};
    bool initialized_{false};
};

}  // namespace plant
