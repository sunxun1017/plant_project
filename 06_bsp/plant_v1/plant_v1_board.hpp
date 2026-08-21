#pragma once

#include <cstddef>
#include <cstdint>

namespace plant::bsp::v1 {

// Plant V1 的所有临时板级参数集中在此文件。上板确认后只修改这里，
// Service、产品行为和 ESP-IDF Adapter 中不得再出现裸 GPIO 或脉宽常量。
struct BoardConfig final {
    struct Gpio final {
        static constexpr int servo_pwm = 4;
        static constexpr int servo_power_enable = 10;
        static constexpr int led_red_pwm = 0;
        static constexpr int led_green_pwm = 1;
        static constexpr int led_blue_pwm = 3;
        static constexpr int vibration_pwm = 6;
        static constexpr int touch_input = 7;
    };

    struct Servo final {
        static constexpr std::uint32_t frequency_hz = 50;
        static constexpr std::uint8_t duty_resolution_bits = 14;
        static constexpr std::uint16_t minimum_pulse_us = 700;
        static constexpr std::uint16_t maximum_pulse_us = 2300;
        static constexpr std::uint16_t neutral_pulse_us = 1500;
        static constexpr std::uint16_t sleep_pulse_us = 900;
        static constexpr std::uint16_t look_up_pulse_us = 1750;
        static constexpr std::uint16_t sway_left_pulse_us = 1250;
        static constexpr std::uint16_t sway_right_pulse_us = 1750;
        static constexpr std::uint32_t estimated_move_time_ms = 700;
        static constexpr bool power_enable_active_high = true;
    };

    struct Led final {
        static constexpr std::uint32_t frequency_hz = 5000;
        static constexpr std::uint8_t duty_resolution_bits = 8;
        static constexpr std::uint8_t maximum_duty = 64;
        static constexpr bool active_high = true;
    };

    struct Vibration final {
        static constexpr std::uint32_t frequency_hz = 200;
        static constexpr std::uint8_t duty_resolution_bits = 8;
        static constexpr std::uint8_t soft_duty = 80;
        static constexpr std::uint8_t warning_duty = 160;
        static constexpr std::uint32_t maximum_continuous_time_ms = 1000;
        static constexpr bool active_high = true;
    };

    struct Touch final {
        static constexpr bool active_high = true;
        static constexpr std::uint32_t debounce_ms = 80;
        static constexpr std::uint32_t long_press_ms = 2000;
        static constexpr bool enable_internal_pull_down = true;
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
        static constexpr char device_name[] = "Plant-V1-C3";
        static constexpr std::uint32_t product_id = 0x504C414EU;  // "PLAN"
        static constexpr std::uint32_t hardware_revision = 1;
        static constexpr std::uint32_t firmware_version = 0x00010000U;
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
    };
};

static_assert(BoardConfig::Servo::minimum_pulse_us < BoardConfig::Servo::sleep_pulse_us);
static_assert(BoardConfig::Servo::sleep_pulse_us < BoardConfig::Servo::neutral_pulse_us);
static_assert(BoardConfig::Servo::neutral_pulse_us < BoardConfig::Servo::maximum_pulse_us);
static_assert(BoardConfig::Servo::look_up_pulse_us <= BoardConfig::Servo::maximum_pulse_us);
static_assert(BoardConfig::Led::maximum_duty <= 255);
static_assert(BoardConfig::Vibration::soft_duty < BoardConfig::Vibration::warning_duty);
static_assert(
    BoardConfig::Ble::advertising_interval_min_units <=
    BoardConfig::Ble::advertising_interval_max_units);

}  // namespace plant::bsp::v1
