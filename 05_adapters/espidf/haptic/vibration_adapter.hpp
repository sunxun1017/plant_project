#pragma once

#include <cstdint>

#include "02_ports/haptic/haptic_port.hpp"

namespace plant {

class EspVibrationAdapter final : public IHapticPort {
public:
    Status initialize();
    Status play(HapticPattern pattern, std::uint32_t execution_id) override;
    Status stop() override;
    void tick(std::uint64_t now_us);

private:
    Status set_duty(std::uint8_t duty);

    bool initialized_{false};
    HapticPattern pattern_{HapticPattern::Off};
    std::uint64_t started_us_{0};
};

}  // namespace plant
