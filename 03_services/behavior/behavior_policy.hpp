#pragma once

#include "01_core/domain/behavior.hpp"

namespace plant {

class BehaviorPolicy {
public:
    [[nodiscard]] static bool try_get_plan(Behavior behavior, BehaviorPlan& plan) noexcept;
    [[nodiscard]] static bool can_interrupt(
        const BehaviorPlan& current,
        Behavior incoming,
        InterruptionReason reason) noexcept;
};

}  // namespace plant
