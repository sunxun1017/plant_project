#pragma once

#include <cstddef>
#include <cstdint>

namespace plant::protocol {

constexpr std::uint8_t kMagic = 0xA5;
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kHeaderSize = 8;
constexpr std::size_t kCrcSize = 4;
constexpr std::size_t kMaximumPayloadSize = 500;
constexpr std::size_t kMaximumFrameSize = kHeaderSize + kMaximumPayloadSize + kCrcSize;
constexpr std::uint8_t kResponseType = 0x80;

[[nodiscard]] std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept;

}  // namespace plant::protocol
