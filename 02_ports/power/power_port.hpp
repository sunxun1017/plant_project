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
    virtual Status enter_light_sleep() = 0;
    virtual Status leave_light_sleep() = 0;
    virtual Status enter_deep_sleep(std::uint64_t timer_wakeup_us) = 0;
    [[nodiscard]] virtual WakeSource wake_source() const = 0;
};

}  // namespace plant
