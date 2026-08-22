#pragma once

#include <cstdint>

#include "02_ports/sensing/climate_sensor_port.hpp"
#include "05_adapters/espidf/sensing/v2_i2c_bus.hpp"

namespace plant {

class Aht21Adapter final : public IClimateSensorPort {
public:
    explicit Aht21Adapter(V2I2cBus& bus) noexcept;

    Status initialize() override;
    Status set_enabled(bool enabled) override;
    Status poll(
        std::uint64_t now_us,
        ClimateSample& sample,
        bool& available) override;

private:
    enum class State : std::uint8_t {
        PowerUp = 0,
        Idle,
        Measuring,
    };

    Status read_status(std::uint8_t& status) const;
    Status start_measurement();
    Status finish_measurement(ClimateSample& sample);
    void record_failure(
        std::uint64_t now_us,
        ClimateSample& sample,
        bool& available) noexcept;
    [[nodiscard]] static std::uint8_t crc8(const std::uint8_t* data, std::size_t size) noexcept;

    V2I2cBus& bus_;
    i2c_master_dev_handle_t device_{nullptr};
    State state_{State::PowerUp};
    std::uint64_t due_us_{0};
    std::uint8_t retry_count_{0};
    bool initialized_{false};
    bool enabled_{true};
};

}  // namespace plant
