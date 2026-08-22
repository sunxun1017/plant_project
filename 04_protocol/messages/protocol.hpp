#pragma once

#include <cstddef>
#include <cstdint>

namespace plant::protocol {

constexpr std::uint8_t kMagic = 0xA5;
constexpr std::uint8_t kVersion1 = 1;
constexpr std::uint8_t kVersion2 = 2;
constexpr std::uint8_t kVersion = kVersion1;
constexpr std::size_t kHeaderSize = 8;
constexpr std::size_t kCrcSize = 4;
constexpr std::size_t kMaximumPayloadSize = 500;
constexpr std::size_t kMaximumFrameSize = kHeaderSize + kMaximumPayloadSize + kCrcSize;
constexpr std::uint8_t kResponseType = 0x80;

enum Capability : std::uint32_t {
    PositionFeedbackCapability = 1U << 0U,
    AcousticActivityCapability = 1U << 1U,
    RelativeIlluminationCapability = 1U << 2U,
    ClimateCapability = 1U << 3U,
    GrowthCapability = 1U << 4U,
    BatteryCapability = 1U << 5U,
    PersistentBondingCapability = 1U << 6U,
};

[[nodiscard]] std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept;

}  // namespace plant::protocol
