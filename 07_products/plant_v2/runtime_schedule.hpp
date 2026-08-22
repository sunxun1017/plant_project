#pragma once

#include <cstdint>

#include "01_core/domain/device_state.hpp"

namespace plant::product::v2 {

struct RuntimeScheduleConfig final {
    std::uint32_t active_tick_ms;
    std::uint32_t fault_tick_ms;
    std::uint32_t sleeping_tick_ms;
};

constexpr std::uint32_t base_runtime_delay_ms(
    DeviceState state,
    bool haptic_active,
    bool sleep_transition_active,
    RuntimeScheduleConfig config) noexcept {
    if (haptic_active || sleep_transition_active) {
        return config.active_tick_ms;
    }
    if (state == DeviceState::Fault) {
        return config.fault_tick_ms;
    }
    if (state == DeviceState::Sleeping) {
        return config.sleeping_tick_ms;
    }
    return config.active_tick_ms;
}

}  // namespace plant::product::v2
