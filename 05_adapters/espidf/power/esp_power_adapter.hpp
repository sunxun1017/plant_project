#pragma once

#include "02_ports/power/power_port.hpp"
#include "esp_pm.h"

namespace plant {

class EspPowerAdapter final : public IPowerPort {
public:
    Status initialize();
    Status enter_light_sleep() override;
    Status leave_light_sleep() override;
    Status enter_deep_sleep(std::uint64_t timer_wakeup_us) override;
    [[nodiscard]] WakeSource wake_source() const override;

private:
    Status configure_touch_wakeup(bool deep_sleep);
    bool initialized_{false};
    bool active_lock_held_{false};
    esp_pm_lock_handle_t active_lock_{nullptr};
};

}  // namespace plant
