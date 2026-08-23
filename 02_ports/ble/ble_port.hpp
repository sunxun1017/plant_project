/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 12:10:54
 * @FilePath: /plant_project/02_ports/ble/ble_port.hpp
 * @Description: 
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "01_core/common/fixed_buffer.hpp"
#include "01_core/common/status.hpp"

namespace plant {

constexpr std::size_t kMaximumBleFrameSize = 512;
using BleFrame = FixedBuffer<kMaximumBleFrameSize>;

class IBlePort {    // 一个通用的ble接口 可具体实现需要继承
public:
    virtual ~IBlePort() = default;
    virtual Status initialize() = 0;
    virtual bool receive(BleFrame& frame) = 0;
    virtual Status send(const std::uint8_t* data, std::size_t size) = 0;
    [[nodiscard]] virtual bool connected() const = 0;
    [[nodiscard]] virtual bool secure() const { return false; }
    [[nodiscard]] virtual bool bonded() const { return false; }
};

}  // namespace plant
