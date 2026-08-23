#pragma once

#include <cstddef>
#include <cstdint>

namespace plant::config::v2 {

// V2 产品策略默认值。这里描述用户可感知的阈值、节奏和通信策略；
// GPIO、电气极性、器件地址与安全机械限位仍由 06_bsp 提供。
struct ProductConfig final {
    struct Behavior final {
        // 闭环舵机只表示生长高度；Happy/Wake/Sleep 等表现只使用灯光与振动。
    };

    struct Acoustic final {
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
        static constexpr std::uint16_t dark_threshold = 180;
        static constexpr std::uint16_t bright_enter_threshold = 700;
        static constexpr std::uint16_t bright_exit_threshold = 600;
        static constexpr std::uint32_t bright_confirm_ms = 5000;
        static constexpr std::uint32_t bright_exit_hold_ms = 2000;
        static constexpr std::uint32_t exposure_credit_interval_ms = 5U * 60U * 1000U;
    };

    struct Climate final {
        static constexpr std::int16_t suitable_min_temperature_centi_c = 1800;
        static constexpr std::int16_t suitable_max_temperature_centi_c = 3000;
        static constexpr std::uint16_t suitable_min_humidity_tenths_percent = 300;
        static constexpr std::uint16_t suitable_max_humidity_tenths_percent = 750;
        static constexpr std::int16_t temperature_hysteresis_centi_c = 50;
        static constexpr std::uint16_t humidity_hysteresis_tenths_percent = 20;
        static constexpr std::uint32_t suitable_credit_interval_ms = 10U * 60U * 1000U;
    };

    struct Battery final {
        static constexpr std::uint16_t low_level_per_mille = 200;
        static constexpr std::uint16_t critical_level_per_mille = 80;
    };

    struct Touch final {
        static constexpr std::uint32_t debounce_ms = 80;
        static constexpr std::uint32_t long_press_ms = 2000;
        static constexpr std::uint32_t factory_reset_hold_ms = 10000;
    };

    struct Growth final {
        static constexpr std::uint16_t step = 50;
        static constexpr std::uint16_t decay_step = 10;
        static constexpr std::uint8_t maximum_pending_credits = 4;
        // RAM 队列最多 4 项，直到执行或复位才清除；避免轻睡眠中的有效互动在唤醒前丢失。
        static constexpr std::uint32_t pending_expiry_ms = 0;
        // 传感 Service 已负责去抖、持续时间片和边沿去重；每个有效事件都获得一个生长量。
        static constexpr std::uint32_t touch_cooldown_ms = 0;
        static constexpr std::uint32_t speech_cooldown_ms = 0;
        static constexpr std::uint32_t sunlight_cooldown_ms = 0;
        static constexpr std::uint32_t climate_cooldown_ms = 0;
        // 暂定 6 小时无有效互动后开始衰减，此后每小时下降一个较小步长；待样机体验标定。
        static constexpr std::uint32_t inactivity_before_decay_ms =
            6U * 60U * 60U * 1000U;
        static constexpr std::uint32_t decay_interval_ms = 60U * 60U * 1000U;
    };

    struct Interaction final {
        static constexpr std::uint32_t automatic_sleep_ms = 5U * 60U * 1000U;
        static constexpr std::uint32_t system_tick_ms = 20;
        // ErrorBlink 每 500 ms 换相；Fault 不再以 20 ms 空转，但仍保留可见故障反馈。
        static constexpr std::uint32_t fault_tick_ms = 500;
        // 入睡后的 1 秒仍用活动 Tick 完成渐灭和短振动，之后降低周期唤醒频率。
        // BLE/触摸会主动通知 System Task，AHT21 的 85 ms 测量截止时间单独参与调度；
        // 稳定睡眠的兜底 Tick 因而可降到 1 秒。整机平均电流仍必须上板实测。
        static constexpr std::uint32_t sleep_transition_ms = 1000;
        static constexpr std::uint32_t sleeping_tick_ms = 1000;
    };

    struct Power final {
        // 首版硬件在完成整机电流、唤醒和 BLE 重连验收前默认不进入深睡，
        // 避免离线后只能依赖触摸唤醒而造成“设备失踪”的体验。
        static constexpr bool deep_sleep_enabled = false;
        static constexpr std::uint32_t deep_sleep_delay_ms = 30U * 60U * 1000U;
        static constexpr std::uint64_t timer_wakeup_us = 0;
    };

    struct Product final {
        static constexpr char device_name[] = "Plant-V2-C3";
        static constexpr std::uint32_t product_id = 0x504C414EU;  // "PLAN"
        static constexpr std::uint32_t hardware_revision = 2;
        static constexpr std::uint32_t firmware_version = 0x00020004U;  // 固件版本
        static constexpr std::size_t ota_chunk_size = 496;
    };

    struct Ble final {
        static constexpr std::uint16_t service_uuid = 0xFFF0;   // service uuid
        static constexpr std::uint16_t command_uuid = 0xFFF1;
        static constexpr std::uint16_t response_uuid = 0xFFF2;
        static constexpr std::uint16_t preferred_mtu = 517;
        static constexpr std::size_t receive_queue_depth = 4;
        // 0.625 ms/单位：启动、唤醒和断连后先快速广播 30 秒，再转入低占空比广播。
        static constexpr std::uint16_t fast_advertising_interval_min_units = 160;
        static constexpr std::uint16_t fast_advertising_interval_max_units = 240;
        static constexpr std::uint32_t fast_advertising_duration_ms = 30000;
        static constexpr std::uint16_t slow_advertising_interval_min_units = 800;
        static constexpr std::uint16_t slow_advertising_interval_max_units = 1600;
        static constexpr std::uint8_t maximum_bonds = 3;
        static constexpr std::uint8_t maximum_connections = 1;
        static constexpr bool secure_connections_only = true;
        static constexpr bool bonding_required = true;
        // 无显示和键盘，当前只能使用 LE Secure Connections Just Works，不具备 MITM 认证。
        static constexpr bool mitm_protection = false;
    };
};

static_assert(
    ProductConfig::Illumination::bright_exit_threshold <
    ProductConfig::Illumination::bright_enter_threshold);
static_assert(
    ProductConfig::Climate::suitable_min_temperature_centi_c <
    ProductConfig::Climate::suitable_max_temperature_centi_c);
static_assert(
    ProductConfig::Climate::suitable_min_humidity_tenths_percent <
    ProductConfig::Climate::suitable_max_humidity_tenths_percent);
static_assert(
    ProductConfig::Touch::factory_reset_hold_ms > ProductConfig::Touch::long_press_ms);
static_assert(ProductConfig::Growth::decay_step < ProductConfig::Growth::step);
static_assert(
    ProductConfig::Growth::maximum_pending_credits > 0 &&
    ProductConfig::Growth::maximum_pending_credits <= 4);
static_assert(ProductConfig::Growth::inactivity_before_decay_ms > 0);
static_assert(ProductConfig::Growth::decay_interval_ms > 0);
static_assert(
    ProductConfig::Interaction::sleeping_tick_ms >=
    ProductConfig::Interaction::system_tick_ms);
static_assert(
    ProductConfig::Interaction::fault_tick_ms >=
    ProductConfig::Interaction::system_tick_ms);
static_assert(
    ProductConfig::Ble::fast_advertising_interval_min_units <=
    ProductConfig::Ble::fast_advertising_interval_max_units);
static_assert(
    ProductConfig::Ble::slow_advertising_interval_min_units <=
    ProductConfig::Ble::slow_advertising_interval_max_units);

}  // namespace plant::config::v2
