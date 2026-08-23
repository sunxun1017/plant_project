#pragma once

#include "01_core/common/status.hpp"
#include "01_core/domain/configuration.hpp"
#include "02_ports/power/power_port.hpp"
#include "03_services/lifecycle/lifecycle_service.hpp"

namespace plant {

class PowerService {
public:
    PowerService(
        LifecycleService& lifecycle,
        IPowerPort& power,
        LowPowerConfig config = {}) noexcept;

    Status request_light_sleep();
    Status request_deep_sleep(const PowerConditions& conditions);
    Status wake_from_low_power();

    [[nodiscard]] WakeSource last_wake_source() const noexcept;
    [[nodiscard]] const LowPowerConfig& config() const noexcept;

private:
    LifecycleService& lifecycle_;
    IPowerPort& power_;
    LowPowerConfig config_;
    WakeSource last_wake_source_{WakeSource::Unknown};
};

}  // namespace plant
