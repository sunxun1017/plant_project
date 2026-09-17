#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace plant::zw_v1 {
constexpr std::uint8_t crc8(std::uint8_t high, std::uint8_t low) {
    std::uint8_t crc = 0xff;
    for (auto byte : {high, low}) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = static_cast<std::uint8_t>((crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1);
        }
    }
    return crc;
}
struct ClimateReading {
    std::int16_t temperature_centi_c{};
    std::uint16_t humidity_tenths_percent{};
    bool valid{};
};
constexpr ClimateReading decode_climate(const std::array<std::uint8_t, 6>& bytes) {
    if (crc8(bytes[0], bytes[1]) != bytes[2] || crc8(bytes[3], bytes[4]) != bytes[5]) return {};
    const std::int32_t raw_t = (bytes[0] << 8) | bytes[1];
    const std::int32_t raw_h = (bytes[3] << 8) | bytes[4];
    return {static_cast<std::int16_t>(-4500 + (17500 * raw_t + 32767) / 65535),
            static_cast<std::uint16_t>(std::clamp<std::int32_t>(-60 + (1250 * raw_h + 32767) / 65535, 0, 1000)), true};
}
static_assert(crc8(0xbe, 0xef) == 0x92);
} // namespace plant::zw_v1
