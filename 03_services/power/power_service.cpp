#include "03_services/power/power_service.hpp"

namespace plant {

PowerService::PowerService(
    LifecycleService& lifecycle,
    IPowerPort& power,
    LowPowerConfig config) noexcept
    : lifecycle_(lifecycle), power_(power), config_(config) {}

Status PowerService::request_light_sleep() {
    Status status = lifecycle_.enter_light_sleep();
    if (!status.ok()) {
        return status;
    }
    status = power_.enter_light_sleep();
    if (!status.ok()) {
        (void)lifecycle_.leave_low_power();
    }
    return status;
}

Status PowerService::request_deep_sleep(const PowerConditions& conditions) {
    if (!config_.deep_sleep_enabled) {
        return Status::failure(ErrorCode::Unsupported);
    }
    if (conditions.ble_connected || conditions.ota_active || conditions.flash_write_active) {
        return Status::failure(ErrorCode::Busy);
    }

    Status status = lifecycle_.enter_deep_sleep();
    if (!status.ok()) {
        return status;
    }
    status = power_.enter_deep_sleep(config_.timer_wakeup_us);
    if (!status.ok()) {
        (void)lifecycle_.leave_low_power();
    }
    return status;
}

Status PowerService::handle_wake() {
    const auto before = lifecycle_.snapshot();
    if (before.state != DeviceState::Sleeping || before.power_mode == PowerMode::Active) {
        return Status::failure(ErrorCode::InvalidState);
    }
    last_wake_source_ = power_.wake_source();
    return lifecycle_.wake(before.power_mode == PowerMode::DeepSleep);
}

WakeSource PowerService::last_wake_source() const noexcept {
    return last_wake_source_;
}

const LowPowerConfig& PowerService::config() const noexcept {
    return config_;
}

}  // namespace plant
