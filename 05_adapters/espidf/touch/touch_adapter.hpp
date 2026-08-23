#pragma once

#include <atomic>
#include <cstdint>

#include "02_ports/touch/touch_port.hpp"
#include "05_adapters/espidf/runtime/runtime_event_signal.hpp"

namespace plant {

struct EspTouchConfig {
    int gpio;
    bool active_high;
    std::uint32_t debounce_ms;
    std::uint32_t minimum_touch_ms;
    bool enable_internal_pull_down;
    EspRuntimeEventSignal runtime_event{};
};

class EspTouchAdapter final : public ITouchPort {
public:
    explicit EspTouchAdapter(EspTouchConfig config) noexcept;
    Status initialize() override;
    bool poll(std::uint64_t now_ms, TouchGesture& gesture) override;
    [[nodiscard]] std::uint32_t next_poll_delay_ms(
        std::uint64_t now_ms,
        std::uint32_t maximum_delay_ms) const noexcept;

private:
    static void gpio_interrupt(void* argument);
    bool read_pressed() const noexcept;
    Status arm_interrupt_for_next_level(bool currently_pressed);

    EspTouchConfig config_;
    bool initialized_{false};
    bool raw_pressed_{false};
    bool stable_pressed_{false};
    bool stable_touch_started_{false};
    std::uint64_t raw_changed_ms_{0};
    std::uint64_t stable_touch_started_ms_{0};
    std::atomic<bool> interrupt_armed_{false};
};

}  // namespace plant
