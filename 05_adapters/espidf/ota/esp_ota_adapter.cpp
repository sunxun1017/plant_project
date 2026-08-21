#include "05_adapters/espidf/ota/esp_ota_adapter.hpp"

namespace plant {

std::size_t EspOtaAdapter::available_image_space() const {
    const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
    return partition == nullptr ? 0 : partition->size;
}

Status EspOtaAdapter::begin(const OtaImageMetadata& metadata) {
    if (active_) {
        return Status::failure(ErrorCode::Busy);
    }
    partition_ = esp_ota_get_next_update_partition(nullptr);
    if (partition_ == nullptr || metadata.image_size > partition_->size) {
        return Status::failure(ErrorCode::OtaFailure);
    }
    if (esp_ota_begin(partition_, metadata.image_size, &handle_) != ESP_OK) {
        partition_ = nullptr;
        return Status::failure(ErrorCode::OtaFailure);
    }
    active_ = true;
    written_ = 0;
    return Status::success();
}

Status EspOtaAdapter::write(
    std::size_t offset,
    const std::uint8_t* data,
    std::size_t size) {
    if (!active_ || data == nullptr || offset != written_) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    if (esp_ota_write(handle_, data, size) != ESP_OK) {
        return Status::failure(ErrorCode::OtaFailure);
    }
    written_ += size;
    return Status::success();
}

Status EspOtaAdapter::verify_and_activate(const OtaImageMetadata&) {
    if (!active_ || partition_ == nullptr) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (esp_ota_end(handle_) != ESP_OK) {
        active_ = false;
        handle_ = 0;
        return Status::failure(ErrorCode::OtaFailure);
    }
    active_ = false;
    handle_ = 0;
    if (esp_ota_set_boot_partition(partition_) != ESP_OK) {
        return Status::failure(ErrorCode::OtaFailure);
    }
    return Status::success();
}

Status EspOtaAdapter::abort() {
    if (!active_) {
        return Status::success();
    }
    const esp_err_t status = esp_ota_abort(handle_);
    active_ = false;
    handle_ = 0;
    partition_ = nullptr;
    written_ = 0;
    return status == ESP_OK ? Status::success() : Status::failure(ErrorCode::OtaFailure);
}

}  // namespace plant
