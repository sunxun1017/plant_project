#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/command.hpp"
#include "01_core/domain/device_state.hpp"
#include "01_core/domain/ota.hpp"
#include "01_core/domain/sensing.hpp"
#include "02_ports/ble/ble_port.hpp"

namespace plant {

struct CommunicationState {
    DeviceState device_state;
    PowerMode power_mode;
    Behavior behavior;
    OtaState ota_state;
    std::uint32_t ota_received_bytes;
    std::uint32_t firmware_version;
    std::uint32_t capabilities{0};
    PositionSnapshot position{};
    AcousticSnapshot acoustic{};
    IlluminationSnapshot illumination{};
    ClimateSnapshot climate{};
    BatterySnapshot battery{};
    GrowthSnapshot growth{};
    ErrorCode active_fault{ErrorCode::None};
    bool ble_secure{false};
    bool ble_bonded{false};
};

class CommunicationService {
public:
    explicit CommunicationService(IBlePort& ble) noexcept;

    Status initialize();
    Status poll(Command& command, bool& available);
    Status respond(
        const Command& command,
        Status execution_status,
        const CommunicationState& state);
    [[nodiscard]] bool connected() const;

private:
    IBlePort& ble_;
};

}  // namespace plant
