#pragma once

#include <cstddef>
#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/ota.hpp"

namespace plant {

class IOtaPort {
public:
    virtual ~IOtaPort() = default;
    virtual Status finalize_boot(bool self_test_ok) = 0;
    [[nodiscard]] virtual std::size_t available_image_space() const = 0;
    virtual Status begin(const OtaImageMetadata& metadata) = 0;
    virtual Status write(
        std::size_t offset,
        const std::uint8_t* data,
        std::size_t size) = 0;
    virtual Status verify_and_activate(const OtaImageMetadata& metadata) = 0;
    virtual Status abort() = 0;
};

}  // namespace plant
