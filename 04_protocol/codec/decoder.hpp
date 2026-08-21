#pragma once

#include <cstddef>
#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/command.hpp"

namespace plant::protocol {

class Decoder {
public:
    [[nodiscard]] static Status decode(
        const std::uint8_t* frame,
        std::size_t frame_size,
        Command& command) noexcept;
};

}  // namespace plant::protocol
