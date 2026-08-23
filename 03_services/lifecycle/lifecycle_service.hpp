/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 18:21:48
 * @FilePath: /plant_project/03_services/lifecycle/lifecycle_service.hpp
 * @Description: 
 */
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
/**
 * @brief 它是总状态机 
 * 不负责具体的灯光之类的 负责整个设备当前的阶段
 */
class LifecycleService {
public:
    /**
     * @brief 返回当前的生命周期和功耗状态 
     * 
     * @return LifecycleSnapshot 
     */
    [[nodiscard]] LifecycleSnapshot snapshot() const noexcept;
    /**
     * @brief 结束boot，赋值生命周期和功耗状态 
     * 
     * @param self_test_passed 
     * @return Status 
     */
    Status finish_boot(bool self_test_passed) noexcept;
    /**
     * @brief 保证它空闲 然后状态是系统正在触发的一个行为中
     * 
     * @param behavior 
     * @return Status 
     */
    Status begin_behavior(Behavior behavior) noexcept;
    /**
     * @brief 把结果转化为生命周期状态
     * 
     * @param outcome 
     * @return Status 
     */
    Status apply_behavior_outcome(BehaviorOutcome outcome) noexcept;
    /**
     * @brief 开启ota升级中 
     * 
     * @return Status 
     */
    Status begin_update() noexcept;
    /**
     * @brief 取消ota升级 
     * 
     * @return Status 
     */
    Status cancel_update() noexcept;
    /**
     * @brief 结束并重启
     * 
     * @return Status 
     */
    Status finish_update_and_reboot() noexcept;
    Status fail_update(bool recoverable) noexcept;

    /**
     * @brief 这里只是赋值状态，并不是真的进入 
     * 
     * @return Status 
     */
    Status enter_light_sleep_mode() noexcept;
    Status enter_deep_sleep_mode() noexcept;
    Status restore_active_power_mode() noexcept;
    Status wake(bool requires_reinitialization) noexcept;
    Status raise_fault() noexcept;
    Status reset_fault() noexcept;

private:
    DeviceState state_{DeviceState::Booting};
    PowerMode power_mode_{PowerMode::Active};
    DeviceState update_return_state_{DeviceState::Idle};
};

}  // namespace plant
