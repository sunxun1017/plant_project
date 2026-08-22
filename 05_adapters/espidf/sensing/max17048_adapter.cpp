#include "05_adapters/espidf/sensing/max17048_adapter.hpp"

#include <algorithm>
#include <array>

#include "06_bsp/plant_v2/plant_v2_board.hpp"

namespace plant {
namespace {

using Config = bsp::v2::BoardConfig;
constexpr std::uint8_t kVcellRegister = 0x02;
constexpr std::uint8_t kSocRegister = 0x04;

}  // namespace

Max17048Adapter::Max17048Adapter(V2I2cBus& bus) noexcept : bus_(bus) {}

Status Max17048Adapter::initialize() {
    const Status status = bus_.add_device(
        Config::Battery::address,
        Config::Climate::i2c_frequency_hz,
        device_);
    if (!status.ok()) {
        return status;
    }
    initialized_ = true;
    next_sample_us_ = 0;
    return Status::success();
}

Status Max17048Adapter::poll(
    std::uint64_t now_us,
    BatterySample& sample,
    bool& available) {
    sample = BatterySample{};
    available = false;
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (now_us < next_sample_us_) {
        return Status::success();
    }
    next_sample_us_ = now_us + Config::Battery::sample_period_ms * 1000ULL;

    std::uint16_t raw_voltage = 0;
    std::uint16_t raw_soc = 0;
    if (!read_register(kVcellRegister, raw_voltage).ok() ||
        !read_register(kSocRegister, raw_soc).ok()) {
        available = true;
        return Status::success();
    }

    // VCELL 的高 12 位每 LSB 为 1.25 mV；SOC 每 LSB 为 1/256 %。
    const std::uint32_t voltage_mv = (raw_voltage >> 4U) * 5U / 4U;
    const std::uint32_t level_per_mille = raw_soc * 10U / 256U;
    sample.voltage_mv = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        voltage_mv, UINT16_MAX));
    sample.level_per_mille = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        level_per_mille, kNormalizedSensorMaximum));
    sample.valid = voltage_mv >= 2000 && voltage_mv <= 5000 && level_per_mille <= 1200;
    available = true;
    return Status::success();
}

Status Max17048Adapter::read_register(std::uint8_t address, std::uint16_t& value) const {
    std::array<std::uint8_t, 2> data{};
    if (i2c_master_transmit_receive(
            device_,
            &address,
            1,
            data.data(),
            data.size(),
            Config::Battery::transfer_timeout_ms) != ESP_OK) {
        return Status::failure(ErrorCode::SensorFailure);
    }
    value = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8U) | data[1]);
    return Status::success();
}

}  // namespace plant
