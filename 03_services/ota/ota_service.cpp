#include "03_services/ota/ota_service.hpp"

namespace plant {

OtaService::OtaService(
    LifecycleService& lifecycle,
    IOtaPort& ota,
    OtaProductIdentity identity,
    bool allow_downgrade) noexcept
    : lifecycle_(lifecycle),
      ota_(ota),
      identity_(identity),
      allow_downgrade_(allow_downgrade) {}

Status OtaService::validate(const OtaImageMetadata& metadata) const noexcept {
    if (metadata.product_id != identity_.product_id ||
        metadata.hardware_revision != identity_.hardware_revision) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    if (!metadata.signed_image || metadata.image_size == 0 ||
        metadata.image_size > ota_.available_image_space()) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    if (!allow_downgrade_ && metadata.firmware_version <= identity_.current_firmware_version) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    return Status::success();
}

Status OtaService::finalize_boot(bool self_test_ok) {
    return ota_.finalize_boot(self_test_ok);
}

void OtaService::schedule_boot_confirmation(
    std::uint64_t now_us,
    std::uint64_t retry_interval_us) noexcept {
    boot_confirmation_reference_us_ = now_us;
    boot_confirmation_retry_interval_us_ = retry_interval_us;
    boot_confirmation_pending_ = true;
}

Status OtaService::poll_boot_confirmation(
    std::uint64_t now_us,
    bool self_test_ok,
    bool& attempted) {
    attempted = false;
    if (boot_confirmation_pending_ && now_us < boot_confirmation_reference_us_) {
        // Rebase after a clock discontinuity; require a full stable interval.
        boot_confirmation_reference_us_ = now_us;
        return Status::success();
    }
    if (!boot_confirmation_pending_ ||
        now_us - boot_confirmation_reference_us_ < boot_confirmation_retry_interval_us_) {
        return Status::success();
    }

    attempted = true;
    const Status status = ota_.finalize_boot(self_test_ok);
    if (status.ok()) {
        boot_confirmation_pending_ = false;
    } else {
        // 保留待确认状态，并从本次失败开始重新计算重试间隔。
        boot_confirmation_reference_us_ = now_us;
    }
    return status;
}

Status OtaService::begin(const OtaImageMetadata& metadata) {
    if (state_ == OtaState::Receiving || state_ == OtaState::Verifying) {
        return Status::failure(ErrorCode::Busy);
    }
    const Status validation = validate(metadata);
    if (!validation.ok()) {
        return validation;
    }

    Status status = lifecycle_.begin_update();
    if (!status.ok()) {
        return status;
    }
    status = ota_.begin(metadata);
    if (!status.ok()) {
        (void)lifecycle_.cancel_update();
        state_ = OtaState::Failed;
        return status;
    }

    metadata_ = metadata;
    received_bytes_ = 0;
    state_ = OtaState::Receiving;
    return Status::success();
}

Status OtaService::write_chunk(
    std::size_t offset,
    const std::uint8_t* data,
    std::size_t size) {
    if (state_ != OtaState::Receiving) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (data == nullptr || size == 0 || offset != received_bytes_ ||
        size > metadata_.image_size - received_bytes_) {
        return Status::failure(ErrorCode::InvalidArgument);
    }

    const Status status = ota_.write(offset, data, size);
    if (!status.ok()) {
        fail_recoverably();
        return status;
    }
    received_bytes_ += size;
    return Status::success();
}

Status OtaService::finish() {
    if (state_ != OtaState::Receiving) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (received_bytes_ != metadata_.image_size) {
        return Status::failure(ErrorCode::InvalidArgument);
    }

    state_ = OtaState::Verifying;
    const Status status = ota_.verify_and_activate(metadata_);
    if (!status.ok()) {
        fail_recoverably();
        return status;
    }

    state_ = OtaState::ReadyToReboot;
    return lifecycle_.finish_update_and_reboot();
}

Status OtaService::cancel() {
    if (state_ != OtaState::Receiving && state_ != OtaState::Verifying) {
        return Status::failure(ErrorCode::InvalidState);
    }
    const Status port_status = ota_.abort();
    const Status lifecycle_status = lifecycle_.cancel_update();
    state_ = port_status.ok() && lifecycle_status.ok() ? OtaState::Idle : OtaState::Failed;
    return !port_status.ok() ? port_status : lifecycle_status;
}

OtaSnapshot OtaService::snapshot() const noexcept {
    return OtaSnapshot{state_, received_bytes_, metadata_.image_size};
}

void OtaService::fail_recoverably() noexcept {
    (void)ota_.abort();
    (void)lifecycle_.fail_update(true);
    state_ = OtaState::Failed;
}

}  // namespace plant
