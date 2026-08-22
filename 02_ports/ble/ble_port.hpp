#pragma once

#include <cstddef>
#include <cstdint>

#include "01_core/common/fixed_buffer.hpp"
#include "01_core/common/status.hpp"

namespace plant {

constexpr std::size_t kMaximumBleFrameSize = 512;
using BleFrame = FixedBuffer<kMaximumBleFrameSize>;

class IBlePort {
public:
    virtual ~IBlePort() = default;
    virtual Status initialize() = 0;
    virtual bool receive(BleFrame& frame) = 0;
    virtual Status send(const std::uint8_t* data, std::size_t size) = 0;
    [[nodiscard]] virtual bool connected() const = 0;
    [[nodiscard]] virtual bool secure() const { return false; }
    [[nodiscard]] virtual bool bonded() const { return false; }
    virtual Status forget_bonds() { return Status::failure(ErrorCode::Unsupported); }
};

}  // namespace plant
