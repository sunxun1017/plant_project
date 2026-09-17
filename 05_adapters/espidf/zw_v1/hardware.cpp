#include "05_adapters/espidf/zw_v1/hardware.hpp"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace plant::zw_v1 {
using namespace bsp::zw_v1;
namespace {
constexpr int timeout_ms = 30;
void delay_ms(unsigned ms) { vTaskDelay(pdMS_TO_TICKS(ms) + 1); }
}
esp_err_t Hardware::add_device(std::uint8_t address, i2c_master_dev_handle_t& device) {
    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = address;
    config.scl_speed_hz = Board::i2c_hz;
    return i2c_master_bus_add_device(bus_, &config, &device);
}
esp_err_t Hardware::initialize() {
#if !CONFIG_ZW_BOARD_POWER_PATH_VERIFIED
    // Do not sink unlimited PNP base current on an unmodified release PCB.
    return ESP_ERR_NOT_SUPPORTED;
#endif
    if (initialized_ || bus_) return ESP_ERR_INVALID_STATE;
    // Hold PWM low before touching the expander. Never initialize UART on GPIO21.
    gpio_set_level(static_cast<gpio_num_t>(Board::servo_pwm), 0);
    gpio_config_t pins{};
    pins.pin_bit_mask = 1ULL << Board::servo_pwm;
    pins.mode = GPIO_MODE_OUTPUT;
    esp_err_t err = gpio_config(&pins);
    if (err != ESP_OK) return err;
    pins = {};
    pins.pin_bit_mask = 1ULL << Board::expander_interrupt;
    pins.mode = GPIO_MODE_INPUT;
    // External R4 is the pull-up. Do not enable a pull-down on this strap pin.
    if ((err = gpio_config(&pins)) != ESP_OK) return err;
    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = static_cast<gpio_num_t>(Board::i2c_sda);
    config.scl_io_num = static_cast<gpio_num_t>(Board::i2c_scl);
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    if ((err = i2c_new_master_bus(&config, &bus_)) != ESP_OK) return err;
    if ((err = add_device(Board::expander_address, expander_)) != ESP_OK) return err;
    // Keep the shared-bus devices powered: their SDA/SCL pins have no shown
    // isolation from the always-on pullups. Hard gating requires a board fix.
    if ((err = write_latch(standby_latch)) != ESP_OK) return err;
    if ((err = add_device(Board::climate_address, climate_)) != ESP_OK) return err;
    if ((err = add_device(Board::charger_address, charger_)) != ESP_OK) return err;
    adc_oneshot_unit_init_cfg_t adc_config{};
    adc_config.unit_id = ADC_UNIT_1;
    if ((err = adc_oneshot_new_unit(&adc_config, &adc_)) != ESP_OK) return err;
    adc_oneshot_chan_cfg_t channel{};
    channel.atten = ADC_ATTEN_DB_12;
    channel.bitwidth = ADC_BITWIDTH_DEFAULT;
    if ((err = adc_oneshot_config_channel(adc_, ADC_CHANNEL_3, &channel)) != ESP_OK) return err;
    initialized_ = true;
    // TTP223 needs 0.5s after POR. Do not interpret its startup level as a touch.
    delay_ms(550);
    bool ignored{};
    return read_touch(ignored);
}
esp_err_t Hardware::write_latch(std::uint8_t value) {
    value = static_cast<std::uint8_t>((value | input_mask) & 0xfe);
    const esp_err_t err = i2c_master_transmit(expander_, &value, 1, timeout_ms);
    // A failed transaction may have changed the physical latch. Do not keep using
    // a stale shadow to enable another actuator after a bus fault.
    latch_known_ = err == ESP_OK;
    if (latch_known_) latch_ = value;
    return err;
}
esp_err_t Hardware::set_output(Output output, bool enabled) {
    if (!healthy()) return ESP_ERR_INVALID_STATE;
    // Charging requires battery/NTC/current configuration not supplied by BOM.
    if (output == Output::Charge && enabled) return ESP_ERR_NOT_SUPPORTED;
    // The rail may be removed only once all dependent controls are OFF.
    if (output == Output::PeripheralRail && !enabled) return ESP_ERR_NOT_SUPPORTED;
    if (output == Output::Audio && !enabled) return ESP_ERR_NOT_SUPPORTED;
    if (output != Output::PeripheralRail && enabled && (latch_ & bit(4))) return ESP_ERR_INVALID_STATE;
    auto err = write_latch(output_latch(latch_, output, enabled));
    if (err == ESP_OK && output == Output::PeripheralRail && enabled) delay_ms(20);
    return err;
}
esp_err_t Hardware::all_loads_off() {
    if (!expander_) return ESP_ERR_INVALID_STATE;
    // Stop servo signal first. Shared I2C rail remains powered to avoid SDA/SCL
    // backfeeding unpowered GXHT40/codec pins. "Off" refers to controlled loads.
    auto err = gpio_set_level(static_cast<gpio_num_t>(Board::servo_pwm), 0);
    if (err != ESP_OK) return err;
    return write_latch(standby_latch);
}
esp_err_t Hardware::read_touch(bool& pressed) {
    pressed = false;
    if (!healthy()) return ESP_ERR_INVALID_STATE;
    std::uint8_t port{};
    const auto err = i2c_master_receive(expander_, &port, 1, timeout_ms);
    if (err == ESP_OK) pressed = touch_pressed(port);
    // Input sample is never used as output latch: P5 may be externally LOW.
    return err;
}
esp_err_t Hardware::read_climate(ClimateReading& reading) {
    reading = {};
    if (!healthy() || (latch_ & bit(4))) return ESP_ERR_INVALID_STATE;
    const std::uint8_t command = 0xfd;
    auto err = i2c_master_transmit(climate_, &command, 1, timeout_ms);
    if (err != ESP_OK) return err;
    delay_ms(10); // high repeatability conversion max 8.3ms (GXHT4x V1.4)
    std::array<std::uint8_t, 6> frame{};
    if ((err = i2c_master_receive(climate_, frame.data(), frame.size(), timeout_ms)) != ESP_OK) return err;
    reading = decode_climate(frame);
    return reading.valid ? ESP_OK : ESP_ERR_INVALID_CRC;
}
esp_err_t Hardware::charger_register(std::uint8_t reg, std::uint8_t& value) {
    return i2c_master_transmit_receive(charger_, &reg, 1, &value, 1, timeout_ms);
}
esp_err_t Hardware::read_charger(ChargerReading& reading) {
    reading = {};
    if (!healthy()) return ESP_ERR_INVALID_STATE;
    auto err = charger_register(0x0a, reading.identity);
    if (err != ESP_OK) return err;
    if ((reading.identity & 0xe0) != 0x20) return ESP_ERR_NOT_SUPPORTED;
    if ((err = charger_register(0x08, reading.status)) != ESP_OK) return err;
    // REG09 is latched: first read history, second read current faults.
    if ((err = charger_register(0x09, reading.latched_fault)) != ESP_OK) return err;
    if ((err = charger_register(0x09, reading.current_fault)) != ESP_OK) return err;
    reading.valid = true;
    return ESP_OK;
}
esp_err_t Hardware::read_position(int& raw) {
    raw = 0;
    if (!healthy() || (latch_ & (bit(3) | bit(4)))) return ESP_ERR_INVALID_STATE;
    return adc_oneshot_read(adc_, ADC_CHANNEL_3, &raw);
}
esp_err_t Hardware::enter_deep_sleep(std::uint64_t timer_us) {
    if (!healthy()) return ESP_ERR_INVALID_STATE;
    auto err = all_loads_off();
    if (err != ESP_OK) return err;
    bool pressed{};
    if ((err = read_touch(pressed)) != ESP_OK) return err;
    // Drain/ack INT, but never sleep over an already active input or stuck IRQ.
    if (pressed || gpio_get_level(static_cast<gpio_num_t>(Board::expander_interrupt)) == 0) return ESP_ERR_INVALID_STATE;
    if ((err = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL)) != ESP_OK) return err;
    if (timer_us && (err = esp_sleep_enable_timer_wakeup(timer_us)) != ESP_OK) return err;
    if ((err = esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(1ULL << Board::expander_interrupt, ESP_GPIO_WAKEUP_GPIO_LOW)) != ESP_OK) return err;
    // If touch arrives after the check, level wake causes immediate boot, not
    // a lost edge. PCF8574 + TTP223 remain supplied by the unswitched ESP_3V3.
    esp_deep_sleep_start();
    return ESP_FAIL;
}
} // namespace plant::zw_v1
