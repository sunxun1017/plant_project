#pragma once
#include <cstdint>
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "06_bsp/zw_v1/board.hpp"
#include "05_adapters/espidf/zw_v1/sensor_codec.hpp"

namespace plant::zw_v1 {
struct ChargerReading {
    std::uint8_t identity{}, status{}, latched_fault{}, current_fault{};
    bool valid{};
    // BQ24259 has status registers, NOT a voltage ADC or a coulomb counter.
};
// One task owns all operations, including PCF8574 output shadow and touch reads.
// Never perform I2C transactions in the GPIO ISR.
class Hardware final {
public:
    esp_err_t initialize();
    esp_err_t set_output(bsp::zw_v1::Output output, bool enabled);
    esp_err_t read_touch(bool& pressed);
    esp_err_t read_climate(ClimateReading& reading);
    esp_err_t read_charger(ChargerReading& reading);
    esp_err_t read_position(int& raw); // requires servo/pot power already enabled
    esp_err_t all_loads_off();
    esp_err_t enter_deep_sleep(std::uint64_t timer_us);
    i2c_master_bus_handle_t bus() const { return bus_; }
    bool healthy() const { return initialized_ && latch_known_; }
private:
    esp_err_t write_latch(std::uint8_t value);
    esp_err_t add_device(std::uint8_t address, i2c_master_dev_handle_t& device);
    esp_err_t charger_register(std::uint8_t reg, std::uint8_t& value);
    i2c_master_bus_handle_t bus_{};
    i2c_master_dev_handle_t expander_{}, climate_{}, charger_{};
    adc_oneshot_unit_handle_t adc_{};
    std::uint8_t latch_{bsp::zw_v1::safe_latch};
    bool latch_known_{false};
    bool initialized_{false};
};
} // namespace plant::zw_v1
