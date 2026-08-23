/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 13:39:21
 * @FilePath: /plant_project/05_adapters/espidf/ble/esp_ble_adapter.hpp
 * @Description: 
 */
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "02_ports/ble/ble_port.hpp"
#include "05_adapters/espidf/runtime/runtime_event_signal.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct ble_gap_event;
struct ble_gatt_access_ctxt;
struct ble_store_status_event;

namespace plant {

struct EspBleConfig {
    const char* device_name;    // 设备名称
    std::uint16_t service_uuid; // 
    std::uint16_t command_uuid;
    std::uint16_t response_uuid;
    std::uint16_t preferred_mtu;
    std::size_t receive_queue_depth;
    std::uint16_t fast_advertising_interval_min_units;
    std::uint16_t fast_advertising_interval_max_units;
    std::uint32_t fast_advertising_duration_ms;
    std::uint16_t slow_advertising_interval_min_units;
    std::uint16_t slow_advertising_interval_max_units;
    EspRuntimeEventSignal runtime_event{};
};

class EspBleAdapter final : public IBlePort {   // 这个才是它的ble具体实现
public:
    explicit EspBleAdapter(EspBleConfig config) noexcept;
    Status initialize() override;
    bool receive(BleFrame& frame) override;
    Status send(const std::uint8_t* data, std::size_t size) override;
    [[nodiscard]] bool connected() const override;
    [[nodiscard]] bool secure() const override;
    [[nodiscard]] bool bonded() const override;
    Status forget_bonds() override;
    Status request_fast_advertising();

    static int gap_event(ble_gap_event* event, void* argument);
    static int gatt_access(
        std::uint16_t connection_handle,
        std::uint16_t attribute_handle,
        ble_gatt_access_ctxt* context,
        void* argument);
    static int store_status(ble_store_status_event* event, void* argument);
    static void on_sync();
    static void on_reset(int reason);
    static void host_task(void* argument);
    static std::uint16_t response_value_handle_;

private:
    struct RxItem {
        std::uint16_t size;
        std::array<std::uint8_t, kMaximumBleFrameSize> data;
    };

    enum class AdvertisingMode : std::uint8_t { Fast, Slow };
    /**
     * @brief 启动ble广播
     * 
     * @param mode 
     * fast: 广播模式 刚开机希望手机快速发现广播时间短
     * slow: 等待连接 降低功耗
     * @return Status 
     */
    Status start_advertising(AdvertisingMode mode);
    bool enqueue(const std::uint8_t* data, std::size_t size);
    void signal_runtime_event() const noexcept;

    static EspBleAdapter* instance_;
    static constexpr std::size_t kMaximumReceiveQueueDepth = 4;
    EspBleConfig config_;
    StaticQueue_t queue_control_{};
    std::array<
        std::uint8_t,
        sizeof(RxItem) * kMaximumReceiveQueueDepth>
        queue_storage_{};
    QueueHandle_t queue_{nullptr};
    std::atomic<bool> connected_{false};
    std::atomic<bool> secure_{false};
    std::atomic<bool> bonded_{false};
    std::atomic<bool> fast_restart_pending_{false};
    std::atomic<std::uint16_t> connection_handle_{0xFFFF};
};

}  // namespace plant
