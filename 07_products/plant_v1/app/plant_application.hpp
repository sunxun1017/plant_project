#pragma once

#include "01_core/common/status.hpp"
#include "01_core/domain/behavior.hpp"
#include "01_core/domain/command.hpp"
#include "01_core/domain/event.hpp"
#include "01_core/domain/ota.hpp"
#include "03_services/behavior/behavior_service.hpp"
#include "03_services/lifecycle/lifecycle_service.hpp"
#include "03_services/ota/ota_service.hpp"
#include "03_services/power/power_service.hpp"

namespace plant {

class PlantApplication {
public:
    PlantApplication(
        BehaviorService& behavior,
        LifecycleService& lifecycle,
        PowerService& power,
        OtaService& ota) noexcept;

    Status finish_boot(bool self_test_passed);
    Status tick(std::uint64_t now_us);
    Status handle_touch(TouchGesture gesture);
    Status handle_communication_connected();
    Status handle_communication_disconnected();
    Status request_behavior(
        Behavior behavior,
        InterruptionReason reason = InterruptionReason::NormalCommand);
    Status stop_behavior();
    Status handle_behavior_event(const BehaviorEvent& event);
    Status handle_idle_timeout();
    Status wake_for_background_motion();
    Status handle_command(const Command& command);

    Status begin_ota(const OtaImageMetadata& metadata);
    Status write_ota_chunk(
        std::size_t offset,
        const std::uint8_t* data,
        std::size_t size);
    Status finish_ota();
    Status cancel_ota();

    [[nodiscard]] LifecycleSnapshot lifecycle_snapshot() const noexcept;
    [[nodiscard]] BehaviorSnapshot behavior_snapshot() const noexcept;
    [[nodiscard]] OtaSnapshot ota_snapshot() const noexcept;

private:
    Status start_behavior(Behavior behavior, InterruptionReason reason);
    Status apply_behavior_outcome(BehaviorOutcome outcome);
    Status apply_pending_behavior_outcome(Status operation_status);

    BehaviorService& behavior_;
    LifecycleService& lifecycle_;
    PowerService& power_;
    OtaService& ota_;
    bool ota_pending_{false};
    OtaImageMetadata pending_ota_{};
};

}  // namespace plant
