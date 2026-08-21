#pragma once

#include <cstddef>
#include <cstdint>

#include "01_core/common/error.hpp"
#include "01_core/domain/behavior.hpp"
#include "01_core/domain/device_state.hpp"
#include "01_core/domain/ota.hpp"

namespace plant::protocol {

struct ResponseMessage {
    std::uint16_t request_id{0};
    std::uint8_t request_type{0};
    ErrorCode status{ErrorCode::None};
    DeviceState device_state{DeviceState::Booting};
    PowerMode power_mode{PowerMode::Active};
    Behavior behavior{Behavior::Calm};
    OtaState ota_state{OtaState::Idle};
    std::uint32_t ota_received_bytes{0};
    std::uint32_t firmware_version{0};
};

}  // namespace plant::protocol
