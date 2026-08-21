#pragma once

#include <cstdint>

#include "02_ports/motion/motion_port.hpp"

namespace plant {

class EspServoAdapter final : public IMotionPort {
public:
    Status initialize();
    Status play(MotionPattern pattern, std::uint32_t execution_id) override;
    Status stop() override;
    Status poll(std::uint64_t now_us, MotionPollResult& result) override;

private:
    Status set_pulse(std::uint16_t pulse_us);
    std::uint16_t pulse_for_phase() const noexcept;
    std::uint8_t phase_count() const noexcept;

    bool initialized_{false};
    bool active_{false};
    MotionPattern pattern_{MotionPattern::ReturnNeutral};
    std::uint8_t phase_{0};
    std::uint32_t execution_id_{0};
    std::uint64_t deadline_us_{0};
};

}  // namespace plant
