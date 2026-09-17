#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "04_protocol/codec/decoder.hpp"
#include "04_protocol/messages/protocol.hpp"

using namespace plant;
namespace p = plant::protocol;

namespace {
void require(bool condition) {
    if (!condition) {
        std::cerr << "Protocol stress invariant failed\n";
        std::exit(EXIT_FAILURE);
    }
}

void seal(std::vector<std::uint8_t>& frame) {
    const auto crc = p::crc32(frame.data(), frame.size() - p::kCrcSize);
    for (unsigned i = 0; i < 4; ++i) {
        frame[frame.size() - 4 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
    }
}

bool valid_shape(unsigned type, std::size_t payload) {
    switch (type) {
        case 1: case 2: case 4: case 0x12: case 0x13: return payload == 0;
        // The zero-filled payload selects WakeUp.
        case 3: return payload == 1;
        case 0x10: return payload == 17;
        case 0x11: return payload >= 5;
        default: return false;
    }
}
}

int main() {
    Command command{};
    require(!p::Decoder::decode(nullptr, 0, command).ok());
    std::size_t structured = 0;
    for (unsigned version = 1; version <= 2; ++version) {
        for (unsigned type = 0; type <= 255; ++type) {
            for (std::size_t size = 0; size <= p::kMaximumPayloadSize; ++size) {
                std::vector<std::uint8_t> frame(p::kHeaderSize + size + p::kCrcSize);
                frame[0] = p::kMagic;
                frame[1] = static_cast<std::uint8_t>(version);
                frame[2] = static_cast<std::uint8_t>(type);
                frame[6] = static_cast<std::uint8_t>(size);
                frame[7] = static_cast<std::uint8_t>(size >> 8);
                seal(frame);
                const auto result = p::Decoder::decode(frame.data(), frame.size(), command);
                require(result.ok() == valid_shape(type, size));
                require(command.ota_data_size <= command.ota_data.size());
                frame.back() ^= 1;
                require(!p::Decoder::decode(frame.data(), frame.size(), command).ok());
                ++structured;
            }
        }
    }
    // Exact-size allocations let ASan catch reads beyond a truncated frame.
    std::uint32_t random = 0x5a17c3u;
    for (unsigned iteration = 0; iteration < 50000; ++iteration) {
        const std::size_t size = iteration % (p::kMaximumFrameSize + 17);
        std::vector<std::uint8_t> bytes(size);
        for (auto& byte : bytes) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            byte = static_cast<std::uint8_t>(random);
        }
        (void)p::Decoder::decode(bytes.data(), bytes.size(), command);
        require(command.ota_data_size <= command.ota_data.size());
    }
    std::cout << structured << " valid-CRC shapes and CRC mutations; 50000 random frames passed\n";
}
