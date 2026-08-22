#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace plant::bsp::v2 {

// V2 原理图冻结前的集中式板级基线。GPIO、ADC 端点和阈值都必须经过
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
        // GPIO18/19 会占用原生 USB-JTAG；当前基线仅保留 UART0 下载路径。
        static constexpr int aht21_sda = 18;
        static constexpr int aht21_scl = 19;
        static constexpr int microphone_power_enable = 8;
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
        static constexpr std::uint16_t neutral = 450;
        static constexpr std::uint16_t sleep = 120;
        static constexpr std::uint16_t look_up = 620;
        static constexpr std::uint16_t sway_left = 330;
        static constexpr std::uint16_t sway_right = 570;
        static constexpr std::uint16_t tolerance = 15;
        static constexpr std::uint16_t minimum_progress = 5;
        static constexpr std::uint8_t stable_sample_count = 4;
        static constexpr std::uint32_t no_progress_timeout_ms = 600;
        static constexpr std::uint32_t motion_timeout_ms = 3500;
        static constexpr std::uint32_t sample_period_ms = 20;
    };

    struct Servo final {
        static constexpr std::uint32_t frequency_hz = 50;
        static constexpr std::uint8_t duty_resolution_bits = 14;
        static constexpr std::uint16_t minimum_pulse_us = 700;
        static constexpr std::uint16_t maximum_pulse_us = 2300;
        static constexpr std::uint16_t neutral_pulse_us = 1500;
        static constexpr bool power_enable_active_high = true;
    };

    struct Microphone final {
        static constexpr bool power_enable_active_high = true;
        // 包络在真正安静时允许接近 0；此低成本链路无法仅凭低电平区分静音与短路。
        static constexpr int valid_raw_minimum = 0;
        static constexpr int valid_raw_maximum = 4085;
        static constexpr std::uint32_t sample_period_ms = 20;
        static constexpr std::uint32_t front_end_settle_ms = 100;
        static constexpr std::uint16_t initial_noise_floor = 80;
        static constexpr std::uint16_t speech_start_margin = 120;
        static constexpr std::uint16_t speech_stop_margin = 70;
        static constexpr std::uint32_t minimum_speech_ms = 300;
        static constexpr std::uint32_t speech_end_hold_ms = 600;
        static constexpr std::uint32_t sustained_window_ms = 30000;
        static constexpr std::uint32_t sustained_required_ms = 20000;
        static constexpr std::uint32_t actuator_recovery_ms = 800;
    };

    struct Illumination final {
        static constexpr std::uint32_t fixed_resistor_ohm = 10000;
        static constexpr std::uint32_t adc_series_resistor_ohm = 1000;
        static constexpr std::uint32_t filter_capacitor_nf = 100;
        static constexpr int valid_raw_minimum = 10;
        static constexpr int valid_raw_maximum = 4085;
        static constexpr bool larger_raw_means_brighter = true;
        static constexpr std::uint32_t sample_period_ms = 100;
        static constexpr std::uint16_t dark_threshold = 180;
        static constexpr std::uint16_t bright_enter_threshold = 700;
        static constexpr std::uint16_t bright_exit_threshold = 600;
        static constexpr std::uint32_t bright_confirm_ms = 5000;
        static constexpr std::uint32_t bright_exit_hold_ms = 2000;
        static constexpr std::uint32_t exposure_credit_interval_ms = 5U * 60U * 1000U;
    };

    struct Climate final {
        static constexpr std::uint8_t address = 0x38;
        static constexpr std::uint32_t i2c_frequency_hz = 100000;
        static constexpr std::uint32_t power_up_delay_ms = 100;
        static constexpr std::uint32_t measurement_time_ms = 85;
        static constexpr std::uint32_t sample_period_ms = 2000;
        static constexpr std::uint32_t transfer_timeout_ms = 20;
        static constexpr std::uint8_t maximum_retries = 3;
        static constexpr bool crc_required = true;
        static constexpr std::int16_t suitable_min_temperature_centi_c = 1800;
        static constexpr std::int16_t suitable_max_temperature_centi_c = 3000;
        static constexpr std::uint16_t suitable_min_humidity_tenths_percent = 300;
        static constexpr std::uint16_t suitable_max_humidity_tenths_percent = 750;
        static constexpr std::int16_t temperature_hysteresis_centi_c = 50;
        static constexpr std::uint16_t humidity_hysteresis_tenths_percent = 20;
        static constexpr std::uint32_t suitable_credit_interval_ms = 10U * 60U * 1000U;
    };

    struct Battery final {
        // V2 使用与 AHT21 共总线的 MAX17048，避免占用 ESP32-C3 启动绑带 ADC GPIO。
        // 电芯型号、容量曲线和低电量阈值仍需按最终电池包验证。
        static constexpr std::uint8_t address = 0x36;
        static constexpr std::uint32_t sample_period_ms = 5000;
        static constexpr std::uint32_t transfer_timeout_ms = 20;
        static constexpr std::uint16_t low_level_per_mille = 200;
        static constexpr std::uint16_t critical_level_per_mille = 80;
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
        static constexpr std::uint32_t debounce_ms = 80;
        static constexpr std::uint32_t long_press_ms = 2000;
        static constexpr std::uint32_t factory_reset_hold_ms = 10000;
        // TTP223 默认持续高电平表示触摸，输出为推挽；灵敏度电容由样机和外壳厚度确定。
        static constexpr bool enable_internal_pull_down = false;
    };

    struct Growth final {
        static constexpr std::uint16_t step = 50;
        static constexpr std::uint16_t maximum_position = Position::safe_maximum;
        static constexpr std::uint16_t limit_tolerance = Position::tolerance;
        static constexpr std::uint32_t pending_expiry_ms = 30000;
        static constexpr std::uint32_t touch_cooldown_ms = 30000;
        static constexpr std::uint32_t speech_cooldown_ms = 2U * 60U * 1000U;
        static constexpr std::uint32_t sunlight_cooldown_ms = 5U * 60U * 1000U;
        static constexpr std::uint32_t climate_cooldown_ms = 10U * 60U * 1000U;
    };

    struct Interaction final {
        static constexpr std::uint32_t automatic_sleep_ms = 5U * 60U * 1000U;
        static constexpr std::uint32_t system_tick_ms = 20;
    };

    struct Power final {
        static constexpr bool deep_sleep_enabled = true;
        static constexpr std::uint32_t deep_sleep_delay_ms = 30U * 60U * 1000U;
        static constexpr std::uint64_t timer_wakeup_us = 0;
        static constexpr int maximum_cpu_frequency_mhz = 160;
        static constexpr int minimum_cpu_frequency_mhz = 40;
    };

    struct Product final {
        static constexpr char device_name[] = "Plant-V2-C3";
        static constexpr std::uint32_t product_id = 0x504C414EU;  // "PLAN"
        static constexpr std::uint32_t hardware_revision = 2;
        static constexpr std::uint32_t firmware_version = 0x00020000U;
        static constexpr std::size_t ota_chunk_size = 496;
    };

    struct Ble final {
        static constexpr std::uint16_t service_uuid = 0xFFF0;
        static constexpr std::uint16_t command_uuid = 0xFFF1;
        static constexpr std::uint16_t response_uuid = 0xFFF2;
        static constexpr std::uint16_t preferred_mtu = 517;
        static constexpr std::size_t receive_queue_depth = 4;
        static constexpr std::uint16_t advertising_interval_min_units = 800;
        static constexpr std::uint16_t advertising_interval_max_units = 1600;
        static constexpr std::uint8_t maximum_bonds = 3;
        static constexpr std::uint8_t maximum_connections = 1;
        static constexpr bool secure_connections_only = true;
        static constexpr bool bonding_required = true;
        // 无显示和键盘，当前只能使用 LE Secure Connections Just Works，不具备 MITM 认证。
        static constexpr bool mitm_protection = false;
    };
};

static_assert(BoardConfig::Adc::position_channel == BoardConfig::Gpio::position_feedback_adc);
static_assert(BoardConfig::Adc::microphone_channel == BoardConfig::Gpio::microphone_envelope_adc);
static_assert(BoardConfig::Adc::illumination_channel == BoardConfig::Gpio::illumination_adc);
static_assert(BoardConfig::Position::safe_minimum < BoardConfig::Position::neutral);
static_assert(BoardConfig::Position::neutral < BoardConfig::Position::safe_maximum);
static_assert(BoardConfig::Position::sleep >= BoardConfig::Position::safe_minimum);
static_assert(BoardConfig::Position::look_up <= BoardConfig::Position::safe_maximum);
static_assert(
    BoardConfig::Illumination::bright_exit_threshold <
    BoardConfig::Illumination::bright_enter_threshold);
static_assert(
    BoardConfig::Climate::suitable_min_temperature_centi_c <
    BoardConfig::Climate::suitable_max_temperature_centi_c);
static_assert(
    BoardConfig::Climate::suitable_min_humidity_tenths_percent <
    BoardConfig::Climate::suitable_max_humidity_tenths_percent);
static_assert(
    BoardConfig::PowerSource::minimum_capacity_mah <=
    BoardConfig::PowerSource::maximum_capacity_mah);
static_assert(
    BoardConfig::PowerSource::default_charge_current_ma <=
    BoardConfig::PowerSource::minimum_capacity_mah);
static_assert(BoardConfig::Touch::factory_reset_hold_ms > BoardConfig::Touch::long_press_ms);

}  // namespace plant::bsp::v2
