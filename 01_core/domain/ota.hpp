#pragma once

#include <cstddef>
#include <cstdint>

namespace plant {

enum class OtaState : std::uint8_t {
    Idle = 0,
    Receiving,
    Verifying,
    ReadyToReboot,
    Failed,
};

struct OtaImageMetadata {
    std::uint32_t product_id;
    std::uint32_t hardware_revision;
    std::uint32_t firmware_version;
    std::size_t image_size;
    bool signed_image;
};

struct OtaSnapshot {
    OtaState state;
    std::size_t received_bytes;
    std::size_t image_size;
};

}  // namespace plant
