#pragma once

#include <cstddef>
#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/ota.hpp"
#include "02_ports/ota/ota_port.hpp"
#include "03_services/lifecycle/lifecycle_service.hpp"

namespace plant {

struct OtaProductIdentity {
    std::uint32_t product_id;
    std::uint32_t hardware_revision;
    std::uint32_t current_firmware_version;
};

class OtaService {
public:
    OtaService(
        LifecycleService& lifecycle,
        IOtaPort& ota,
        OtaProductIdentity identity,
        bool allow_downgrade = false) noexcept;

    [[nodiscard]] Status validate(const OtaImageMetadata& metadata) const noexcept;
    Status finalize_boot(bool self_test_ok);
    void schedule_boot_confirmation(
        std::uint64_t now_us,
        std::uint64_t retry_interval_us) noexcept;
    Status poll_boot_confirmation(
        std::uint64_t now_us,
        bool self_test_ok,
        bool& attempted);
    Status begin(const OtaImageMetadata& metadata);
    Status write_chunk(
        std::size_t offset,
        const std::uint8_t* data,
        std::size_t size);
    Status finish();
    Status cancel();

    [[nodiscard]] OtaSnapshot snapshot() const noexcept;

private:
    void fail_recoverably() noexcept;

    LifecycleService& lifecycle_;
    IOtaPort& ota_;
    OtaProductIdentity identity_;
    bool allow_downgrade_{false};
    OtaState state_{OtaState::Idle};
    OtaImageMetadata metadata_{};
    std::size_t received_bytes_{0};
    std::uint64_t boot_confirmation_reference_us_{0};
    std::uint64_t boot_confirmation_retry_interval_us_{0};
    bool boot_confirmation_pending_{false};
};

}  // namespace plant
