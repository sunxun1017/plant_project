#pragma once

#include <cstdint>

#include "02_ports/touch/touch_port.hpp"

namespace plant {

class EspTouchAdapter final : public ITouchPort {
public:
    Status initialize() override;
    bool poll(std::uint64_t now_ms, TouchGesture& gesture) override;

private:
    bool read_pressed() const noexcept;

    bool initialized_{false};
    bool raw_pressed_{false};
    bool stable_pressed_{false};
    bool long_press_reported_{false};
    std::uint64_t raw_changed_ms_{0};
    std::uint64_t pressed_since_ms_{0};
};

}  // namespace plant
