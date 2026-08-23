#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace plant::bsp::v2 {

template <std::size_t Size>
constexpr bool pins_are_unique(const std::array<int, Size>& pins) {
    for (std::size_t left = 0; left < Size; ++left) {
        for (std::size_t right = left + 1; right < Size; ++right) {
            if (pins[left] == pins[right]) {
                return false;
            }
        }
    }
    return true;
}

// 状态：全部待定（TBD）。以下 GPIO、ADC 通道、器件/外设实现方式、电气端点、
// 极性、时序和安全限制都是原理图冻结前的临时基线，尚未经过最终 BOM、PCB 和样机
// 验证；不得把它们当作量产硬件结论。
// V2 原理图冻结前的集中式板级基线。GPIO、ADC 电气端点和器件时序都必须经过
// 00_docs/testing/board_bringup_v2.md 的样机流程标定后才能用于量产。
struct BoardConfig final {
    struct Hardware final {
        static constexpr char microphone[] = "ECM + low-noise amplifier + envelope detector";
        static constexpr char illumination_sensor[] = "GL5528 + 10k divider";
        static constexpr char touch_sensor[] = "TTP223 + copper electrode";
        static constexpr char actuator[] = "PWM servo + external linear potentiometer";
        static constexpr char vibration_motor[] = "0827 coin motor + low-side MOSFET + flyback";
        static constexpr char led[] = "common-cathode 5050 RGB + three low-side PWM drivers";
        static constexpr char charger[] = "TP4056 charger + protected cell + load sharing";
        static constexpr char battery[] = "3.7V 300-500mAh LiPo";
        static constexpr char climate_sensor[] = "AHT21";
        static constexpr char fuel_gauge[] = "MAX17048";
    };

    struct Gpio final {
        static constexpr int position_feedback_adc = 0;
        static constexpr int microphone_envelope_adc = 1;
        static constexpr int illumination_adc = 3;
        static constexpr int servo_pwm = 4;
        // GPIO10 和 GPIO8 都有启动期复用/绑带约束；外部使能电路不得改变要求的上电电平。
        static constexpr int servo_power_enable = 10;
        static constexpr int led_red_pwm = 5;
        static constexpr int led_green_pwm = 6;
        static constexpr int led_blue_pwm = 7;
        // GPIO20/21 也是 UART0 TX/RX。量产板必须给下载夹具与负载做串阻/隔离；
        // 下载和运行时振动/触摸不能同时使用 UART0。
        static constexpr int vibration_pwm = 20;
        static constexpr int touch_input = 21;
        // I²C 上拉使 GPIO2/9 在正常启动时保持高电平；BOOT 按键必须仍能把 GPIO9
        // 强制拉低进入下载模式，AHT21/MAX17048 在复位采样期间不得主动拉低总线。
        // GPIO18/19 专用于原生 USB Serial/JTAG，不能再分配给板载外设。
        static constexpr int aht21_sda = 2;
        static constexpr int aht21_scl = 9;
        static constexpr int microphone_power_enable = 8;

        static constexpr std::array<int, 13> assigned{
            position_feedback_adc,
            microphone_envelope_adc,
            illumination_adc,
            servo_pwm,
            servo_power_enable,
            led_red_pwm,
            led_green_pwm,
            led_blue_pwm,
            vibration_pwm,
            touch_input,
            aht21_sda,
            aht21_scl,
            microphone_power_enable,
        };
    };

    struct Adc final {
        static constexpr int position_channel = 0;
        static constexpr int microphone_channel = 1;
        static constexpr int illumination_channel = 3;
        static constexpr std::array<int, 3> channels{
            position_channel,
            microphone_channel,
            illumination_channel,
        };
        static constexpr std::uint32_t sample_period_ms = 20;
        // 三路输入必须被外部电路限制在 0–3.3 V；固件使用 ADC1 12 dB 衰减。
        static constexpr std::uint16_t maximum_input_mv = 3300;
        static constexpr int raw_minimum = 0;
        static constexpr int raw_maximum = 4095;
    };

    struct Position final {
        // 电位器全行程中的保守默认范围；样机校准后要给电气端点保留余量。
        static constexpr int valid_raw_minimum = 160;
        static constexpr int valid_raw_maximum = 3935;
        // 反馈节点有弱上拉：接近低轨按短路处理，接近高轨按滑臂开路处理。
        static constexpr int rail_low_raw_maximum = 40;
        static constexpr int rail_high_raw_minimum = 4055;
        static constexpr bool increasing_raw_means_growing = true;
        static constexpr std::uint16_t safe_minimum = 100;
        static constexpr std::uint16_t safe_maximum = 900;
        static constexpr std::uint16_t tolerance = 15;
        static constexpr std::uint16_t minimum_progress = 5;
        static constexpr std::uint8_t stable_sample_count = 4;
        // 静止时连续异常达到该次数才确认反馈故障；运动时仍在首个异常立即断电。
        // 暂定值需用舵机、RGB、振动和 BLE 同时工作时的 ADC 噪声数据验证。
        static constexpr std::uint8_t idle_invalid_sample_count = 3;
        static constexpr std::uint32_t no_progress_timeout_ms = 600;
        static constexpr std::uint32_t motion_timeout_ms = 3500;
        static constexpr std::uint32_t sample_period_ms = 20;
    };

    struct Servo final {
        static constexpr std::uint32_t frequency_hz = 50;
        static constexpr std::uint8_t duty_resolution_bits = 14;
        static constexpr std::uint16_t minimum_pulse_us = 700;
        static constexpr std::uint16_t maximum_pulse_us = 2300;
        static constexpr bool power_enable_active_high = true;
    };

    struct Microphone final {
        static constexpr bool power_enable_active_high = true;
        // 包络在真正安静时允许接近 0；此低成本链路无法仅凭低电平区分静音与短路。
        static constexpr int valid_raw_minimum = 0;
        static constexpr int valid_raw_maximum = 4085;
        static constexpr std::uint32_t sample_period_ms = 20;
        static constexpr std::uint32_t front_end_settle_ms = 100;
    };

    struct Illumination final {
        static constexpr std::uint32_t fixed_resistor_ohm = 10000;
        static constexpr std::uint32_t adc_series_resistor_ohm = 1000;
        static constexpr std::uint32_t filter_capacitor_nf = 100;
        static constexpr int valid_raw_minimum = 10;
        static constexpr int valid_raw_maximum = 4085;
        static constexpr bool larger_raw_means_brighter = true;
        static constexpr std::uint32_t sample_period_ms = 100;
    };

    struct Climate final {
        static constexpr std::uint8_t address = 0x38;
        static constexpr std::uint32_t i2c_frequency_hz = 100000;
        static constexpr std::uint32_t power_up_delay_ms = 100;
        static constexpr std::uint32_t measurement_time_ms = 85;
        static constexpr std::uint32_t sample_period_ms = 2000;
        static constexpr std::uint32_t transfer_timeout_ms = 20;
        static constexpr std::uint8_t maximum_retries = 3;
        // 连续失败后降低探测频率，避免未焊/损坏的可选传感器持续唤醒 CPU 和 I²C。
        static constexpr std::uint32_t fault_retry_period_ms = 30000;
        static constexpr bool crc_required = true;
    };

    struct Battery final {
        // V2 使用与 AHT21 共总线的 MAX17048，避免占用 ESP32-C3 启动绑带 ADC GPIO。
        // 电芯型号、容量曲线和低电量阈值仍需按最终电池包验证。
        static constexpr std::uint8_t address = 0x36;
        static constexpr std::uint32_t sample_period_ms = 5000;
        static constexpr std::uint32_t transfer_timeout_ms = 20;
        static constexpr std::uint16_t nominal_voltage_mv = 3700;
        static constexpr std::uint16_t minimum_capacity_mah = 300;
        static constexpr std::uint16_t maximum_capacity_mah = 500;
        // TP4056 的 CHRG/STDBY 当前未接 MCU；充电状态不由固件推断。
        static constexpr bool charger_status_connected = false;
    };

    struct PowerSource final {
        // TP4056 不是负载共享芯片，也不自带升/降压；以下条件由原理图实现而非固件补偿。
        static constexpr std::uint16_t cell_nominal_voltage_mv = 3700;
        static constexpr std::uint16_t cell_maximum_charge_voltage_mv = 4200;
        static constexpr std::uint16_t minimum_capacity_mah = 300;
        static constexpr std::uint16_t maximum_capacity_mah = 500;
        static constexpr std::uint16_t default_charge_current_ma = 200;
        static constexpr bool protected_cell_required = true;
        static constexpr bool load_sharing_required = true;
        static constexpr bool servo_rail_bulk_capacitor_required = true;
    };

    struct Led final {
        static constexpr std::uint32_t frequency_hz = 5000;
        static constexpr std::uint8_t duty_resolution_bits = 8;
        static constexpr std::uint8_t maximum_duty = 64;
        static constexpr bool active_high = true;
        // 当前固件选择普通 5050 RGB；WS2812B 是另一个 BOM 变体，不能复用本三路 LEDC BSP。
        static constexpr bool addressable = false;
        static constexpr bool external_current_drivers_required = true;
    };

    struct Vibration final {
        static constexpr std::uint32_t frequency_hz = 200;
        static constexpr std::uint8_t duty_resolution_bits = 8;
        static constexpr std::uint8_t soft_duty = 80;
        static constexpr std::uint8_t warning_duty = 160;
        static constexpr std::uint32_t maximum_continuous_time_ms = 1000;
        static constexpr bool active_high = true;
        static constexpr bool external_low_side_driver_required = true;
        static constexpr bool flyback_protection_required = true;
    };

    struct Touch final {
        static constexpr bool active_high = true;
        // TTP223 默认持续高电平表示触摸，输出为推挽；弱下拉保证传感器断开时不会把悬空
        // 输入误判为长按恢复。灵敏度电容由样机和外壳厚度确定。
        static constexpr bool enable_internal_pull_down = true;
    };

    struct Power final {
        static constexpr int maximum_cpu_frequency_mhz = 160;
        static constexpr int minimum_cpu_frequency_mhz = 40;
    };
};

static_assert(BoardConfig::Adc::position_channel == BoardConfig::Gpio::position_feedback_adc);
static_assert(BoardConfig::Adc::microphone_channel == BoardConfig::Gpio::microphone_envelope_adc);
static_assert(BoardConfig::Adc::illumination_channel == BoardConfig::Gpio::illumination_adc);
static_assert(pins_are_unique(BoardConfig::Gpio::assigned));
static_assert(BoardConfig::Gpio::aht21_sda != 18 && BoardConfig::Gpio::aht21_sda != 19);
static_assert(BoardConfig::Gpio::aht21_scl != 18 && BoardConfig::Gpio::aht21_scl != 19);
static_assert(BoardConfig::Gpio::aht21_sda < 12 || BoardConfig::Gpio::aht21_sda > 17);
static_assert(BoardConfig::Gpio::aht21_scl < 12 || BoardConfig::Gpio::aht21_scl > 17);
static_assert(BoardConfig::Position::safe_minimum < BoardConfig::Position::safe_maximum);
static_assert(BoardConfig::Position::idle_invalid_sample_count > 0);
static_assert(BoardConfig::Climate::maximum_retries > 0);
static_assert(
    BoardConfig::Climate::fault_retry_period_ms >= BoardConfig::Climate::sample_period_ms);
static_assert(
    BoardConfig::PowerSource::minimum_capacity_mah <=
    BoardConfig::PowerSource::maximum_capacity_mah);
static_assert(
    BoardConfig::PowerSource::default_charge_current_ma <=
    BoardConfig::PowerSource::minimum_capacity_mah);
}  // namespace plant::bsp::v2
