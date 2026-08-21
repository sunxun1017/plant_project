#pragma once

#include "01_core/common/fixed_buffer.hpp"
#include "01_core/common/status.hpp"
#include "04_protocol/messages/protocol.hpp"
#include "04_protocol/messages/telemetry_message.hpp"

namespace plant::protocol {

class Encoder {
public:
    [[nodiscard]] static Status encode_response(
        const ResponseMessage& response,
        FixedBuffer<kMaximumFrameSize>& frame) noexcept;
};

}  // namespace plant::protocol
