#pragma once

#include <cstdint>

#include "02_ports/sensing/battery_sensor_port.hpp"
#include "05_adapters/espidf/sensing/v2_i2c_bus.hpp"

namespace plant {

class Max17048Adapter final : public IBatterySensorPort {
public:
    explicit Max17048Adapter(V2I2cBus& bus) noexcept;

    Status initialize() override;
    Status poll(
        std::uint64_t now_us,
        BatterySample& sample,
        bool& available) override;

private:
    Status read_register(std::uint8_t address, std::uint16_t& value) const;

    V2I2cBus& bus_;
    i2c_master_dev_handle_t device_{nullptr};
    std::uint64_t next_sample_us_{0};
    bool initialized_{false};
};

}  // namespace plant
