#pragma once

#include <cstddef>

#include "02_ports/ota/ota_port.hpp"
#include "esp_ota_ops.h"

namespace plant {

class EspOtaAdapter final : public IOtaPort {
public:
    Status finalize_boot(bool self_test_ok) override;
    [[nodiscard]] std::size_t available_image_space() const override;
    Status begin(const OtaImageMetadata& metadata) override;
    Status write(
        std::size_t offset,
        const std::uint8_t* data,
        std::size_t size) override;
    Status verify_and_activate(const OtaImageMetadata& metadata) override;
    Status abort() override;

private:
    const esp_partition_t* partition_{nullptr};
    esp_ota_handle_t handle_{0};
    std::size_t written_{0};
    bool active_{false};
};

}  // namespace plant
