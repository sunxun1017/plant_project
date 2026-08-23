#include "04_protocol/codec/decoder.hpp"

#include "04_protocol/messages/protocol.hpp"

namespace plant::protocol {
namespace {

std::uint16_t read_u16(const std::uint8_t* data) noexcept {
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(data[1] << 8U);
}

std::uint32_t read_u32(const std::uint8_t* data) noexcept {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8U) |
           (static_cast<std::uint32_t>(data[2]) << 16U) |
           (static_cast<std::uint32_t>(data[3]) << 24U);
}

bool no_payload(std::size_t size) noexcept {
    return size == 0;
}

bool valid_behavior_wire_value(std::uint8_t value) noexcept {
    return value == static_cast<std::uint8_t>(Behavior::WakeUp) ||
           value == static_cast<std::uint8_t>(Behavior::Happy) ||
           value == static_cast<std::uint8_t>(Behavior::Sleep) ||
           value == static_cast<std::uint8_t>(Behavior::Error);
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

Status Decoder::decode(
    const std::uint8_t* frame,
    std::size_t frame_size,
    Command& command) noexcept {
    if (frame == nullptr || frame_size < kHeaderSize + kCrcSize ||
        frame_size > kMaximumFrameSize) {
        return Status::failure(ErrorCode::ProtocolFailure);
    }
    if (frame[0] != kMagic ||
        (frame[1] != kVersion1 && frame[1] != kVersion2) || frame[3] != 0) {
        return Status::failure(ErrorCode::ProtocolFailure);
    }

    const std::size_t payload_size = read_u16(frame + 6);
    if (payload_size > kMaximumPayloadSize ||
        frame_size != kHeaderSize + payload_size + kCrcSize) {
        return Status::failure(ErrorCode::ProtocolFailure);
    }
    const std::uint32_t expected_crc = read_u32(frame + kHeaderSize + payload_size);
    if (crc32(frame, kHeaderSize + payload_size) != expected_crc) {
        return Status::failure(ErrorCode::ProtocolFailure);
    }

    command = Command{};
    command.protocol_version = frame[1];
    command.request_id = read_u16(frame + 4);
    command.type = static_cast<CommandType>(frame[2]);
    const std::uint8_t* payload = frame + kHeaderSize;

    switch (command.type) {
        case CommandType::Ping:
        case CommandType::GetState:
        case CommandType::StopBehavior:
        case CommandType::FinishOta:
        case CommandType::CancelOta:
            return no_payload(payload_size) ? Status::success()
                                            : Status::failure(ErrorCode::InvalidArgument);
        case CommandType::SetBehavior:
            // 线上协议只允许普通语义行为；V2 Grow/Retract 由 Growth Service 触发，
            // 不让客户端用枚举值绕过位置闭环、有界排队和安全限幅。
            if (payload_size != 1 || !valid_behavior_wire_value(payload[0])) {
                return Status::failure(ErrorCode::InvalidArgument);
            }
            command.behavior = static_cast<Behavior>(payload[0]);
            return Status::success();
        case CommandType::BeginOta:
            if (payload_size != 17 || payload[16] > 1) {
                return Status::failure(ErrorCode::InvalidArgument);
            }
            command.ota_metadata = OtaImageMetadata{
                read_u32(payload),
                read_u32(payload + 4),
                read_u32(payload + 8),
                read_u32(payload + 12),
                payload[16] == 1,
            };
            return Status::success();
        case CommandType::OtaChunk:
            if (payload_size <= 4 || payload_size - 4 > kMaximumOtaChunkSize) {
                return Status::failure(ErrorCode::InvalidArgument);
            }
            command.ota_offset = read_u32(payload);
            command.ota_data_size = payload_size - 4;
            for (std::size_t index = 0; index < command.ota_data_size; ++index) {
                command.ota_data[index] = payload[index + 4];
            }
            return Status::success();
    }
    return Status::failure(ErrorCode::Unsupported);
}

}  // namespace plant::protocol
