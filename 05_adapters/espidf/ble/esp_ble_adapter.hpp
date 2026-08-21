#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "02_ports/ble/ble_port.hpp"
#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct ble_gap_event;
struct ble_gatt_access_ctxt;

namespace plant {

class EspBleAdapter final : public IBlePort {
public:
    Status initialize() override;
    bool receive(BleFrame& frame) override;
    Status send(const std::uint8_t* data, std::size_t size) override;
    [[nodiscard]] bool connected() const override;

    static int gap_event(ble_gap_event* event, void* argument);
    static int gatt_access(
        std::uint16_t connection_handle,
        std::uint16_t attribute_handle,
        ble_gatt_access_ctxt* context,
        void* argument);
    static void on_sync();
    static void on_reset(int reason);
    static void host_task(void* argument);
    static std::uint16_t response_value_handle_;

private:
    struct RxItem {
        std::uint16_t size;
        std::array<std::uint8_t, kMaximumBleFrameSize> data;
    };

    Status start_advertising();
    bool enqueue(const std::uint8_t* data, std::size_t size);

    static EspBleAdapter* instance_;
    StaticQueue_t queue_control_{};
    std::array<
        std::uint8_t,
        sizeof(RxItem) * bsp::v1::BoardConfig::Ble::receive_queue_depth>
        queue_storage_{};
    QueueHandle_t queue_{nullptr};
    std::atomic<bool> connected_{false};
    std::atomic<std::uint16_t> connection_handle_{0xFFFF};
};

}  // namespace plant
