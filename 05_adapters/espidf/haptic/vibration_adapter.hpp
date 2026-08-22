#pragma once

#include <cstdint>

#include "02_ports/haptic/haptic_port.hpp"

namespace plant {

struct EspVibrationConfig {
    int gpio;
    std::uint32_t frequency_hz;
    std::uint8_t duty_resolution_bits;
    std::uint8_t soft_duty;
    std::uint8_t warning_duty;
    std::uint32_t maximum_continuous_time_ms;
    bool active_high;
};

class EspVibrationAdapter final : public IHapticPort {
public:
    EspVibrationAdapter() noexcept;
    explicit EspVibrationAdapter(EspVibrationConfig config) noexcept;
    Status initialize();
    Status play(HapticPattern pattern, std::uint32_t execution_id) override;
    Status stop() override;
    Status tick(std::uint64_t now_us) override;

private:
    Status set_duty(std::uint8_t duty);

    EspVibrationConfig config_;
    bool initialized_{false};
    HapticPattern pattern_{HapticPattern::Off};
    std::uint64_t started_us_{0};
};

}  // namespace plant
