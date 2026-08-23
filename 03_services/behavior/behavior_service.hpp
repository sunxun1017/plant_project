#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/behavior.hpp"
#include "01_core/domain/configuration.hpp"
#include "01_core/domain/event.hpp"
#include "02_ports/haptic/haptic_port.hpp"
#include "02_ports/light/light_port.hpp"
#include "02_ports/motion/motion_port.hpp"

namespace plant {

enum class BehaviorRunState : std::uint8_t {
    Idle = 0,
    Running,
    Fault,
};

struct BehaviorSnapshot {
    BehaviorRunState state;
    Behavior behavior;
    std::uint32_t execution_id;
};

class BehaviorService {
public:
    BehaviorService(
        IMotionPort& motion,
        ILightPort& light,
        IHapticPort& haptic,
        BehaviorExecutionConfig config = {}) noexcept;

    Status start(
        Behavior behavior,
        InterruptionReason reason = InterruptionReason::NormalCommand);
    Status stop();
    Status tick(std::uint64_t now_us);
    Status handle_event(const BehaviorEvent& event);

    [[nodiscard]] BehaviorSnapshot snapshot() const noexcept;
    [[nodiscard]] BehaviorOutcome take_outcome() noexcept;

private:
    Status start_plan(const BehaviorPlan& plan);
    Status enter_fault_with(ErrorCode error) noexcept;
    void stop_outputs() noexcept;
    void stop_non_motion_outputs() noexcept;
    void enter_fault() noexcept;
    Status complete_current();
    [[nodiscard]] std::uint32_t next_execution_id() noexcept;

    IMotionPort& motion_;
    ILightPort& light_;
    IHapticPort& haptic_;
    BehaviorExecutionConfig config_;
    BehaviorRunState state_{BehaviorRunState::Idle};
    BehaviorPlan current_plan_{};
    Behavior current_behavior_{Behavior::None};
    std::uint32_t execution_id_{0};
    BehaviorOutcome outcome_{BehaviorOutcome::None};
    std::uint64_t started_us_{0};
    bool start_time_initialized_{false};
};

}  // namespace plant
