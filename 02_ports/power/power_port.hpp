/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 18:04:08
 * @FilePath: /plant_project/02_ports/power/power_port.hpp
 * @Description: 
 */
#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/device_state.hpp"

namespace plant {

enum class WakeSource : std::uint8_t {
    Unknown = 0,
    Touch,
    Ble,
    Timer,
    Reset,
    PowerOn,
};

class IPowerPort {
public:
    virtual ~IPowerPort() = default;
    /**
     * @brief 允许进入睡眠
     * 
     * @return Status 
     */
    virtual Status allow_light_sleep() = 0;
    virtual Status restore_active_mode() = 0;
    virtual Status enter_deep_sleep(std::uint64_t timer_wakeup_us) = 0;
    [[nodiscard]] virtual WakeSource wake_source() const = 0;
};

}  // namespace plant
