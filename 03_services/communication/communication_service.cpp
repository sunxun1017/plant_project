#include "03_services/communication/communication_service.hpp"

#include "04_protocol/codec/decoder.hpp"
#include "04_protocol/codec/encoder.hpp"

namespace plant {

static_assert(kMaximumBleFrameSize == protocol::kMaximumFrameSize);

CommunicationService::CommunicationService(IBlePort& ble) noexcept : ble_(ble) {}

Status CommunicationService::initialize() {
    return ble_.initialize();
}

Status CommunicationService::poll(Command& command, bool& available) {
    BleFrame frame;
    available = ble_.receive(frame);
    if (!available) {
        return Status::success();
    }
    return protocol::Decoder::decode(frame.data(), frame.size(), command);
}

Status CommunicationService::respond(
    const Command& command,
    Status execution_status,
    const CommunicationState& state) {
    protocol::ResponseMessage response{};
    response.request_id = command.request_id;
    response.request_type = static_cast<std::uint8_t>(command.type);
    response.status = execution_status.code();
    response.device_state = state.device_state;
    response.power_mode = state.power_mode;
    response.behavior = state.behavior;
    response.ota_state = state.ota_state;
    response.ota_received_bytes = state.ota_received_bytes;
    response.firmware_version = state.firmware_version;

    FixedBuffer<protocol::kMaximumFrameSize> frame;
    const Status encode_status = protocol::Encoder::encode_response(response, frame);
    if (!encode_status.ok()) {
        return encode_status;
    }
    return ble_.send(frame.data(), frame.size());
}

bool CommunicationService::connected() const {
    return ble_.connected();
}

}  // namespace plant
