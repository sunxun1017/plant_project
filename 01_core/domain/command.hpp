#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "01_core/domain/behavior.hpp"
#include "01_core/domain/ota.hpp"

namespace plant {

constexpr std::size_t kMaximumOtaChunkSize = 496;

enum class CommandType : std::uint8_t {
    Ping = 0x01,
    GetState = 0x02,
    SetBehavior = 0x03,
    StopBehavior = 0x04,
    BeginOta = 0x10,
    OtaChunk = 0x11,
    FinishOta = 0x12,
    CancelOta = 0x13,
    ForgetBonds = 0x20,
};

struct Command {
    CommandType type{CommandType::Ping};
    std::uint16_t request_id{0};
    std::uint8_t protocol_version{1};
    Behavior behavior{Behavior::Calm};
    OtaImageMetadata ota_metadata{};
    std::uint32_t ota_offset{0};
    std::array<std::uint8_t, kMaximumOtaChunkSize> ota_data{};
    std::size_t ota_data_size{0};
};

}  // namespace plant
