#pragma once

#include <cstdint>

#include "02_ports/touch/touch_port.hpp"

namespace plant {

struct EspTouchConfig {
    int gpio;
    bool active_high;
    std::uint32_t debounce_ms;
    std::uint32_t long_press_ms;
    std::uint32_t factory_reset_hold_ms;
    bool enable_internal_pull_down;
};

class EspTouchAdapter final : public ITouchPort {
public:
    explicit EspTouchAdapter(EspTouchConfig config) noexcept;
    Status initialize() override;
    bool poll(std::uint64_t now_ms, TouchGesture& gesture) override;

private:
    bool read_pressed() const noexcept;

    EspTouchConfig config_;
    bool initialized_{false};
    bool raw_pressed_{false};
    bool stable_pressed_{false};
    bool long_press_reported_{false};
    bool factory_reset_reported_{false};
    std::uint64_t raw_changed_ms_{0};
    std::uint64_t pressed_since_ms_{0};
};

}  // namespace plant
