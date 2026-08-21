#pragma once

#include "02_ports/power/power_port.hpp"

namespace plant {

class EspPowerAdapter final : public IPowerPort {
public:
    Status enter_light_sleep() override;
    Status enter_deep_sleep(std::uint64_t timer_wakeup_us) override;
    [[nodiscard]] WakeSource wake_source() const override;

private:
    Status configure_touch_wakeup(bool deep_sleep);
};

}  // namespace plant
