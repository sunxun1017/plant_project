#pragma once

#include <array>
#include <cstdint>

#include "01_core/domain/sensing.hpp"

namespace plant {

struct BehaviorExecutionConfig {
    std::uint64_t timeout_us{5ULL * 1000ULL * 1000ULL};
    bool expressive_motion_enabled{true};
};

struct LowPowerConfig {
    bool deep_sleep_enabled{true};
    std::uint32_t deep_sleep_delay_ms{30U * 60U * 1000U};
    std::uint64_t timer_wakeup_us{0};
};

struct PowerConditions {
    bool ble_connected{false};
    bool ota_active{false};
    bool flash_write_active{false};
};

struct AcousticDetectionConfig {
    std::uint16_t initial_noise_floor{80};
    std::uint16_t speech_start_margin{120};
    std::uint16_t speech_stop_margin{70};
    std::uint64_t minimum_speech_us{300ULL * 1000ULL};
    std::uint64_t speech_end_hold_us{600ULL * 1000ULL};
    std::uint64_t sustained_window_us{30ULL * 1000ULL * 1000ULL};
    std::uint64_t sustained_required_us{20ULL * 1000ULL * 1000ULL};
};

struct IlluminationDetectionConfig {
    std::uint16_t dark_threshold{180};
    std::uint16_t bright_enter_threshold{700};
    std::uint16_t bright_exit_threshold{600};
    std::uint64_t bright_confirm_us{5ULL * 1000ULL * 1000ULL};
    std::uint64_t bright_exit_hold_us{2ULL * 1000ULL * 1000ULL};
    std::uint64_t exposure_credit_interval_us{5ULL * 60ULL * 1000ULL * 1000ULL};
};

struct ClimateDetectionConfig {
    std::int16_t suitable_min_temperature_centi_c{1800};
    std::int16_t suitable_max_temperature_centi_c{3000};
    std::uint16_t suitable_min_humidity_tenths_percent{300};
    std::uint16_t suitable_max_humidity_tenths_percent{750};
    std::int16_t temperature_hysteresis_centi_c{50};
    std::uint16_t humidity_hysteresis_tenths_percent{20};
    std::uint64_t suitable_credit_interval_us{10ULL * 60ULL * 1000ULL * 1000ULL};
};

struct GrowthConfig {
    std::uint16_t step{50};
    std::uint16_t maximum_position{900};
    std::uint16_t limit_tolerance{10};
    std::uint64_t pending_expiry_us{30ULL * 1000ULL * 1000ULL};
    // 下标与 GrowthSource 的枚举值一致；None 的冷却时间始终忽略。
    std::array<std::uint64_t, 5> cooldown_us{
        0,
        30ULL * 1000ULL * 1000ULL,
        2ULL * 60ULL * 1000ULL * 1000ULL,
        5ULL * 60ULL * 1000ULL * 1000ULL,
        10ULL * 60ULL * 1000ULL * 1000ULL,
    };
    std::uint16_t minimum_position{100};
    std::uint16_t decay_step{10};
    std::uint64_t inactivity_before_decay_us{6ULL * 60ULL * 60ULL * 1000ULL * 1000ULL};
    std::uint64_t decay_interval_us{60ULL * 60ULL * 1000ULL * 1000ULL};
    std::uint8_t maximum_pending_credits{4};
};

}  // namespace plant
