/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 18:06:11
 * @FilePath: /plant_project/03_services/power/power_service.cpp
 * @Description: 
 */
#include "03_services/power/power_service.hpp"

namespace plant {

PowerService::PowerService(
    LifecycleService& lifecycle,
    IPowerPort& power,
    LowPowerConfig config) noexcept
    : lifecycle_(lifecycle), power_(power), config_(config) {}

Status PowerService::request_light_sleep() {
    Status status = lifecycle_.enter_light_sleep_mode(); // 这个是生命管理进入睡眠
    if (!status.ok()) {
        return status;
    }
    status = power_.allow_light_sleep();    // 这个锁释放后，ESP-IDF的电源管理系统会在CPU空闲时自动进入Light Sleep
    if (!status.ok()) {
        (void)lifecycle_.restore_active_power_mode();
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

    Status status = lifecycle_.enter_deep_sleep_mode();
    if (!status.ok()) {
        return status;
    }
    status = power_.enter_deep_sleep(config_.timer_wakeup_us);
    if (!status.ok()) {
        (void)lifecycle_.restore_active_power_mode();
    }
    return status;
}

Status PowerService::wake_from_low_power() {
    const auto before = lifecycle_.snapshot();
    if (before.state != DeviceState::Sleeping || before.power_mode == PowerMode::Active) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (before.power_mode == PowerMode::LightSleep) {
        const Status active_status = power_.restore_active_mode();
        if (!active_status.ok()) {
            return active_status;
        }
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
