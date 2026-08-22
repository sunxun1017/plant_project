#pragma once

#include <cstdint>

#include "02_ports/motion/position_motion_port.hpp"
#include "05_adapters/espidf/sensing/v2_adc_sampler.hpp"

namespace plant {

class FeedbackServoAdapter final : public IPositionMotionPort {
public:
    explicit FeedbackServoAdapter(V2AdcSampler& adc) noexcept;

    Status initialize();
    Status play(MotionPattern pattern, std::uint32_t execution_id) override;
    Status move_to(std::uint16_t target_position, std::uint32_t execution_id) override;
    Status stop() override;
    Status poll(std::uint64_t now_us, MotionPollResult& result) override;
    [[nodiscard]] PositionSnapshot position_snapshot() const noexcept override;

private:
    Status start_target(std::uint16_t target_position);
    Status set_target_pwm(std::uint16_t target_position);
    Status sample_position(std::uint64_t now_us);
    Status fail_motion(MotionFault fault);

    V2AdcSampler& adc_;
    PositionSnapshot snapshot_{};
    std::uint32_t execution_id_{0};
    std::uint64_t motion_started_us_{0};
    std::uint64_t last_progress_us_{0};
    std::uint64_t next_sample_us_{0};
    std::uint16_t last_position_{0};
    std::uint16_t progress_position_{0};
    std::uint8_t stable_samples_{0};
    std::uint8_t opposite_samples_{0};
    std::int8_t expected_direction_{0};
    bool initialized_{false};
};

}  // namespace plant
