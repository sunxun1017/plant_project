#include "04_protocol/codec/encoder.hpp"

namespace plant::protocol {
namespace {

void write_u16(std::uint8_t* output, std::uint16_t value) noexcept {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(std::uint8_t* output, std::uint32_t value) noexcept {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8U);
    output[2] = static_cast<std::uint8_t>(value >> 16U);
    output[3] = static_cast<std::uint8_t>(value >> 24U);
}

}  // namespace

Status Encoder::encode_response(
    const ResponseMessage& response,
    FixedBuffer<kMaximumFrameSize>& frame) noexcept {
    constexpr std::size_t payload_size = 14;
    constexpr std::size_t frame_size = kHeaderSize + payload_size + kCrcSize;
    frame.clear();
    if (!frame.resize(frame_size)) {
        return Status::failure(ErrorCode::InternalFailure);
    }

    std::uint8_t* output = frame.data();
    output[0] = kMagic;
    output[1] = kVersion;
    output[2] = kResponseType;
    output[3] = 0;
    write_u16(output + 4, response.request_id);
    write_u16(output + 6, payload_size);
    output[8] = response.request_type;
    output[9] = static_cast<std::uint8_t>(response.status);
    output[10] = static_cast<std::uint8_t>(response.device_state);
    output[11] = static_cast<std::uint8_t>(response.power_mode);
    output[12] = static_cast<std::uint8_t>(response.behavior);
    output[13] = static_cast<std::uint8_t>(response.ota_state);
    write_u32(output + 14, response.ota_received_bytes);
    write_u32(output + 18, response.firmware_version);
    write_u32(output + kHeaderSize + payload_size, crc32(output, kHeaderSize + payload_size));
    return Status::success();
}

}  // namespace plant::protocol
