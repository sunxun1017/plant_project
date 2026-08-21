#pragma once

#include "01_core/common/status.hpp"
#include "01_core/domain/behavior.hpp"
#include "01_core/domain/device_state.hpp"
#include "01_core/domain/event.hpp"

namespace plant {

struct LifecycleSnapshot {
    DeviceState state;
    PowerMode power_mode;
};

class LifecycleService {
public:
    [[nodiscard]] LifecycleSnapshot snapshot() const noexcept;

    Status finish_boot(bool self_test_passed) noexcept;
    Status begin_behavior(Behavior behavior) noexcept;
    Status apply_behavior_outcome(BehaviorOutcome outcome) noexcept;

    Status begin_update() noexcept;
    Status cancel_update() noexcept;
    Status finish_update_and_reboot() noexcept;
    Status fail_update(bool recoverable) noexcept;

    Status enter_light_sleep() noexcept;
    Status enter_deep_sleep() noexcept;
    Status leave_low_power() noexcept;
    Status wake(bool requires_reinitialization) noexcept;
    Status raise_fault() noexcept;
    Status reset_fault() noexcept;

private:
    DeviceState state_{DeviceState::Booting};
    PowerMode power_mode_{PowerMode::Active};
};

}  // namespace plant
