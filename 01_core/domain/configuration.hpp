#pragma once

#include <cstdint>

namespace plant {

struct BehaviorExecutionConfig {
    std::uint64_t timeout_us{5ULL * 1000ULL * 1000ULL};
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

}  // namespace plant
