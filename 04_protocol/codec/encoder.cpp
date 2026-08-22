#include "04_protocol/codec/encoder.hpp"

#include <algorithm>

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

void write_i16(std::uint8_t* output, std::int16_t value) noexcept {
    write_u16(output, static_cast<std::uint16_t>(value));
}

}  // namespace

Status Encoder::encode_response(
    const ResponseMessage& response,
    FixedBuffer<kMaximumFrameSize>& frame) noexcept {
    constexpr std::size_t v1_payload_size = 14;
    constexpr std::size_t v2_payload_size = 54;
    if (response.protocol_version != kVersion1 &&
        response.protocol_version != kVersion2) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    const std::size_t payload_size = response.protocol_version == kVersion2
                                         ? v2_payload_size
                                         : v1_payload_size;
    const std::size_t frame_size = kHeaderSize + payload_size + kCrcSize;
    frame.clear();
    if (!frame.resize(frame_size)) {
        return Status::failure(ErrorCode::InternalFailure);
    }

    std::uint8_t* output = frame.data();
    output[0] = kMagic;
    output[1] = response.protocol_version;
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
    if (response.protocol_version == kVersion2) {
        write_u32(output + 22, response.capabilities);
        write_u16(output + 26, response.position.actual_position);
        write_u16(output + 28, response.position.target_position);
        output[30] = static_cast<std::uint8_t>(response.position.feedback);
        output[31] = static_cast<std::uint8_t>(response.position.fault);
        output[32] = static_cast<std::uint8_t>(
            (response.position.moving ? 1U : 0U) |
            (response.position.target_reached ? 2U : 0U));
        output[33] = static_cast<std::uint8_t>(response.acoustic.state);
        write_u16(output + 34, response.acoustic.volume_level);
        write_u16(output + 36, response.acoustic.noise_floor);
        write_u16(
            output + 38,
            static_cast<std::uint16_t>(
                std::min<std::uint32_t>(response.acoustic.speaking_duration_ms / 1000U,
                                        UINT16_MAX)));
        output[40] = static_cast<std::uint8_t>(response.illumination.state);
        write_u16(output + 41, response.illumination.relative_level);
        write_u16(
            output + 43,
            static_cast<std::uint16_t>(
                std::min<std::uint32_t>(response.illumination.bright_duration_ms / 1000U,
                                        UINT16_MAX)));
        output[45] = static_cast<std::uint8_t>(response.climate.state);
        write_i16(output + 46, response.climate.temperature_centi_c);
        write_u16(output + 48, response.climate.relative_humidity_tenths_percent);
        write_u16(
            output + 50,
            static_cast<std::uint16_t>(
                std::min<std::uint32_t>(response.climate.suitable_duration_ms / 1000U,
                                        UINT16_MAX)));
        output[52] = static_cast<std::uint8_t>(response.battery.state);
        write_u16(output + 53, response.battery.level_per_mille);
        write_u16(output + 55, response.battery.voltage_mv);
        output[57] = static_cast<std::uint8_t>(response.growth.recent_source);
        output[58] = static_cast<std::uint8_t>(response.growth.pending_source);
        output[59] = static_cast<std::uint8_t>(
            (response.growth.pending ? 1U : 0U) |
            (response.growth.at_limit ? 2U : 0U) |
            (std::min<std::uint8_t>(response.growth.pending_count, 7U) << 2U));
        output[60] = static_cast<std::uint8_t>(
            (response.ble_secure ? 1U : 0U) |
            (response.ble_bonded ? 2U : 0U));
        output[61] = static_cast<std::uint8_t>(response.active_fault);
    }
    write_u32(output + kHeaderSize + payload_size, crc32(output, kHeaderSize + payload_size));
    return Status::success();
}

}  // namespace plant::protocol
