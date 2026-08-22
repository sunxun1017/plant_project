#include <cstddef>
#include <cstdint>

#include "04_protocol/codec/decoder.hpp"
#include "04_protocol/codec/encoder.hpp"
#include "03_services/communication/communication_service.hpp"

namespace plant::test {
namespace {

using protocol::kCrcSize;
using protocol::kHeaderSize;
using protocol::kMagic;
using protocol::kMaximumFrameSize;
using protocol::kVersion;

int failures = 0;

class FakeBle final : public IBlePort {
public:
    Status initialize() override { return Status::success(); }
    bool receive(BleFrame& frame) override {
        if (!has_frame) {
            return false;
        }
        frame = incoming;
        has_frame = false;
        return true;
    }
    Status send(const std::uint8_t* data, std::size_t size) override {
        sent.clear();
        return sent.append(data, size) ? Status::success()
                                       : Status::failure(ErrorCode::InternalFailure);
    }
    bool connected() const override { return true; }

    BleFrame incoming;
    BleFrame sent;
    bool has_frame{false};
};

#define CHECK_PROTOCOL(condition)                                                            \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            ++failures;                                                                      \
        }                                                                                    \
    } while (false)

void write_u16(std::uint8_t* output, std::uint16_t value) {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(std::uint8_t* output, std::uint32_t value) {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8U);
    output[2] = static_cast<std::uint8_t>(value >> 16U);
    output[3] = static_cast<std::uint8_t>(value >> 24U);
}

FixedBuffer<kMaximumFrameSize> make_request(
    CommandType type,
    std::uint16_t request_id,
    const std::uint8_t* payload,
    std::size_t payload_size,
    std::uint8_t version = kVersion) {
    FixedBuffer<kMaximumFrameSize> frame;
    (void)frame.resize(kHeaderSize + payload_size + kCrcSize);
    auto* output = frame.data();
    output[0] = kMagic;
    output[1] = version;
    output[2] = static_cast<std::uint8_t>(type);
    output[3] = 0;
    write_u16(output + 4, request_id);
    write_u16(output + 6, static_cast<std::uint16_t>(payload_size));
    for (std::size_t index = 0; index < payload_size; ++index) {
        output[kHeaderSize + index] = payload[index];
    }
    write_u32(
        output + kHeaderSize + payload_size,
        protocol::crc32(output, kHeaderSize + payload_size));
    return frame;
}

void test_set_behavior_decoding() {
    const std::uint8_t payload[]{static_cast<std::uint8_t>(Behavior::Happy)};
    auto frame = make_request(CommandType::SetBehavior, 42, payload, sizeof(payload));
    Command command;

    CHECK_PROTOCOL(protocol::Decoder::decode(frame.data(), frame.size(), command).ok());
    CHECK_PROTOCOL(command.type == CommandType::SetBehavior);
    CHECK_PROTOCOL(command.request_id == 42);
    CHECK_PROTOCOL(command.behavior == Behavior::Happy);
}

void test_ota_begin_and_chunk_decoding() {
    std::uint8_t begin_payload[17]{};
    write_u32(begin_payload, 0x504C414E);
    write_u32(begin_payload + 4, 1);
    write_u32(begin_payload + 8, 2);
    write_u32(begin_payload + 12, 1024);
    begin_payload[16] = 1;
    auto begin = make_request(CommandType::BeginOta, 7, begin_payload, sizeof(begin_payload));
    Command command;
    CHECK_PROTOCOL(protocol::Decoder::decode(begin.data(), begin.size(), command).ok());
    CHECK_PROTOCOL(command.ota_metadata.product_id == 0x504C414E);
    CHECK_PROTOCOL(command.ota_metadata.image_size == 1024);
    CHECK_PROTOCOL(command.ota_metadata.signed_image);

    const std::uint8_t chunk_payload[]{4, 0, 0, 0, 0xAA, 0xBB, 0xCC};
    auto chunk = make_request(CommandType::OtaChunk, 8, chunk_payload, sizeof(chunk_payload));
    CHECK_PROTOCOL(protocol::Decoder::decode(chunk.data(), chunk.size(), command).ok());
    CHECK_PROTOCOL(command.ota_offset == 4);
    CHECK_PROTOCOL(command.ota_data_size == 3);
    CHECK_PROTOCOL(command.ota_data[2] == 0xCC);
}

void test_crc_and_payload_validation() {
    auto frame = make_request(CommandType::Ping, 1, nullptr, 0);
    frame.data()[0] ^= 1;
    Command command;
    CHECK_PROTOCOL(protocol::Decoder::decode(frame.data(), frame.size(), command).code() ==
                   ErrorCode::ProtocolFailure);

    const std::uint8_t invalid_behavior[]{0xFF};
    frame = make_request(CommandType::SetBehavior, 2, invalid_behavior, 1);
    CHECK_PROTOCOL(protocol::Decoder::decode(frame.data(), frame.size(), command).code() ==
                   ErrorCode::InvalidArgument);
}

void test_forget_bonds_requires_empty_payload() {
    Command command{};
    auto frame = make_request(CommandType::ForgetBonds, 9, nullptr, 0, protocol::kVersion2);
    CHECK_PROTOCOL(protocol::Decoder::decode(frame.data(), frame.size(), command).ok());
    CHECK_PROTOCOL(command.type == CommandType::ForgetBonds);

    const std::uint8_t unexpected_payload[]{1};
    frame = make_request(
        CommandType::ForgetBonds,
        10,
        unexpected_payload,
        sizeof(unexpected_payload),
        protocol::kVersion2);
    CHECK_PROTOCOL(protocol::Decoder::decode(frame.data(), frame.size(), command).code() ==
                   ErrorCode::InvalidArgument);
}

void test_response_encoding() {
    protocol::ResponseMessage response{};
    response.request_id = 99;
    response.request_type = static_cast<std::uint8_t>(CommandType::GetState);
    response.device_state = DeviceState::Sleeping;
    response.power_mode = PowerMode::LightSleep;
    response.behavior = Behavior::Sleep;
    response.ota_state = OtaState::Receiving;
    response.ota_received_bytes = 496;
    response.firmware_version = 0x00010000;
    FixedBuffer<kMaximumFrameSize> frame;

    CHECK_PROTOCOL(protocol::Encoder::encode_response(response, frame).ok());
    CHECK_PROTOCOL(frame[0] == kMagic);
    CHECK_PROTOCOL(frame[2] == protocol::kResponseType);
    CHECK_PROTOCOL(frame[4] == 99);
    const std::size_t payload_size = 14;
    const std::uint32_t encoded_crc =
        static_cast<std::uint32_t>(frame[kHeaderSize + payload_size]) |
        (static_cast<std::uint32_t>(frame[kHeaderSize + payload_size + 1]) << 8U) |
        (static_cast<std::uint32_t>(frame[kHeaderSize + payload_size + 2]) << 16U) |
        (static_cast<std::uint32_t>(frame[kHeaderSize + payload_size + 3]) << 24U);
    CHECK_PROTOCOL(encoded_crc == protocol::crc32(frame.data(), kHeaderSize + payload_size));
}

void test_v2_request_and_extended_response() {
    auto request = make_request(CommandType::GetState, 200, nullptr, 0, protocol::kVersion2);
    Command command{};
    CHECK_PROTOCOL(protocol::Decoder::decode(
                       request.data(), request.size(), command)
                       .ok());
    CHECK_PROTOCOL(command.protocol_version == protocol::kVersion2);

    protocol::ResponseMessage response{};
    response.protocol_version = protocol::kVersion2;
    response.request_id = 200;
    response.request_type = static_cast<std::uint8_t>(CommandType::GetState);
    response.capabilities = protocol::PositionFeedbackCapability |
                            protocol::ClimateCapability |
                            protocol::PersistentBondingCapability;
    response.position = PositionSnapshot{
        420, 475, PositionFeedbackState::Valid, MotionFault::None, true, false};
    response.acoustic.state = AcousticState::Speaking;
    response.acoustic.volume_level = 640;
    response.illumination.state = IlluminationState::BrightExposure;
    response.illumination.relative_level = 810;
    response.climate = ClimateSnapshot{
        ClimateState::Suitable, 2350, 566, 12000, true};
    response.battery = BatterySnapshot{BatteryState::Normal, 3920, 730, true};
    response.growth.recent_source = GrowthSource::Touch;
    response.ble_secure = true;
    response.ble_bonded = true;
    FixedBuffer<kMaximumFrameSize> frame;
    CHECK_PROTOCOL(protocol::Encoder::encode_response(response, frame).ok());
    CHECK_PROTOCOL(frame[1] == protocol::kVersion2);
    CHECK_PROTOCOL(frame[6] == 54);
    CHECK_PROTOCOL(frame.size() == protocol::kHeaderSize + 54 + protocol::kCrcSize);
    CHECK_PROTOCOL(frame[26] == 0xA4 && frame[27] == 0x01);  // actual_position=420
    CHECK_PROTOCOL(frame[40] == static_cast<std::uint8_t>(
                                      IlluminationState::BrightExposure));
    CHECK_PROTOCOL(frame[60] == 0x03);  // encrypted + bonded
}

void test_communication_service_round_trip() {
    FakeBle ble;
    const auto request = make_request(CommandType::Ping, 123, nullptr, 0);
    CHECK_PROTOCOL(ble.incoming.append(request.data(), request.size()));
    ble.has_frame = true;
    CommunicationService communication{ble};
    Command command;
    bool available = false;

    CHECK_PROTOCOL(communication.initialize().ok());
    CHECK_PROTOCOL(communication.poll(command, available).ok());
    CHECK_PROTOCOL(available);
    CHECK_PROTOCOL(command.type == CommandType::Ping);
    CHECK_PROTOCOL(communication.respond(
                       command,
                       Status::success(),
                       CommunicationState{
                           DeviceState::Idle,
                           PowerMode::Active,
                           Behavior::Calm,
                           OtaState::Idle,
                           0,
                           0x00010000,
                       })
                       .ok());
    CHECK_PROTOCOL(!ble.sent.empty());
    CHECK_PROTOCOL(ble.sent[2] == protocol::kResponseType);
    CHECK_PROTOCOL(ble.sent[4] == 123);
}

void test_communication_service_preserves_header_for_parameter_errors() {
    FakeBle ble;
    const std::uint8_t invalid_behavior[]{0xFF};
    const auto request = make_request(CommandType::SetBehavior, 321, invalid_behavior, 1);
    CHECK_PROTOCOL(ble.incoming.append(request.data(), request.size()));
    ble.has_frame = true;
    CommunicationService communication{ble};
    Command command;
    bool available = false;

    CHECK_PROTOCOL(communication.poll(command, available).code() == ErrorCode::InvalidArgument);
    CHECK_PROTOCOL(available);
    CHECK_PROTOCOL(command.type == CommandType::SetBehavior);
    CHECK_PROTOCOL(command.request_id == 321);
}

}  // namespace

int run_protocol_codec_tests() {
    test_set_behavior_decoding();
    test_ota_begin_and_chunk_decoding();
    test_crc_and_payload_validation();
    test_forget_bonds_requires_empty_payload();
    test_response_encoding();
    test_v2_request_and_extended_response();
    test_communication_service_round_trip();
    test_communication_service_preserves_header_for_parameter_errors();
    return failures;
}

}  // namespace plant::test
