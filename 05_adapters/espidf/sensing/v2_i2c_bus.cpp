#include "05_adapters/espidf/sensing/v2_i2c_bus.hpp"

#include "06_bsp/plant_v2/plant_v2_board.hpp"

namespace plant {

Status V2I2cBus::initialize() {
    using Config = bsp::v2::BoardConfig;
    if (initialized_) {
        return Status::success();
    }
    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = static_cast<gpio_num_t>(Config::Gpio::aht21_sda);
    config.scl_io_num = static_cast<gpio_num_t>(Config::Gpio::aht21_scl);
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&config, &bus_) != ESP_OK) {
        return Status::failure(ErrorCode::SensorFailure);
    }
    initialized_ = true;
    return Status::success();
}

Status V2I2cBus::add_device(
    std::uint8_t address,
    std::uint32_t frequency_hz,
    i2c_master_dev_handle_t& device) const {
    if (!initialized_ || bus_ == nullptr) {
        return Status::failure(ErrorCode::InvalidState);
    }
    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = address;
    config.scl_speed_hz = frequency_hz;
    return i2c_master_bus_add_device(bus_, &config, &device) == ESP_OK
               ? Status::success()
               : Status::failure(ErrorCode::SensorFailure);
}

}  // namespace plant
