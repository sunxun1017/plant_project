#include "05_adapters/espidf/ble/esp_ble_adapter.hpp"

#include <cstring>
#include <limits>

#include "esp_log.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

namespace plant {
namespace {

constexpr char kTag[] = "plant_ble";
ble_uuid16_t service_uuid = BLE_UUID16_INIT(0);
ble_uuid16_t command_uuid = BLE_UUID16_INIT(0);
ble_uuid16_t response_uuid = BLE_UUID16_INIT(0);

ble_gatt_chr_def characteristics[3]{};
ble_gatt_svc_def services[2]{};

}  // namespace

extern "C" void ble_store_config_init(void);

EspBleAdapter* EspBleAdapter::instance_ = nullptr;
std::uint16_t EspBleAdapter::response_value_handle_ = 0;

EspBleAdapter::EspBleAdapter(EspBleConfig config) noexcept : config_(config) {}

Status EspBleAdapter::initialize() {
    if (instance_ != nullptr || config_.device_name == nullptr ||
        config_.receive_queue_depth == 0 ||
        config_.receive_queue_depth > kMaximumReceiveQueueDepth ||
        config_.fast_advertising_interval_min_units == 0 ||
        config_.fast_advertising_interval_min_units >
            config_.fast_advertising_interval_max_units ||
        config_.slow_advertising_interval_min_units == 0 ||
        config_.slow_advertising_interval_min_units >
            config_.slow_advertising_interval_max_units ||
        config_.fast_advertising_duration_ms >
            static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
        return Status::failure(ErrorCode::InvalidState);
    }
    queue_ = xQueueCreateStatic(
        config_.receive_queue_depth,
        sizeof(RxItem),
        queue_storage_.data(),
        &queue_control_);
    if (queue_ == nullptr) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    instance_ = this;

    // 绑定密钥位于 NVS。初始化异常时进入故障，绝不为“自动恢复”整分区擦除密钥；
    // 用户仍可通过明确的重新烧录/NVS 恢复流程处理损坏分区。
    const esp_err_t nvs_status = nvs_flash_init();
    if (nvs_status != ESP_OK) {
        instance_ = nullptr;
        return Status::failure(ErrorCode::StorageFailure);
    }
    if (nimble_port_init() != ESP_OK) {
        instance_ = nullptr;
        return Status::failure(ErrorCode::InternalFailure);
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    // 不能使用 NimBLE 示例的 round-robin 策略；它会在绑定表满时静默删除最旧设备。
    ble_hs_cfg.store_status_cb = store_status;
    ble_hs_cfg.store_status_arg = this;
    ble_svc_gap_init();
    ble_svc_gatt_init();

    service_uuid.value = config_.service_uuid;
    command_uuid.value = config_.command_uuid;
    response_uuid.value = config_.response_uuid;

    characteristics[0].uuid = &command_uuid.u;
    characteristics[0].access_cb = gatt_access;
    characteristics[0].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP |
                               BLE_GATT_CHR_F_WRITE_ENC;
    characteristics[1].uuid = &response_uuid.u;
    // NimBLE 要求每个 Characteristic 都提供 access_cb，即使该值只用于服务端通知。
    // CCCD 写入要求加密，实际发送还会再次检查 connected/secure/bonded。
    characteristics[1].access_cb = gatt_access;
    characteristics[1].val_handle = &response_value_handle_;
    characteristics[1].flags =
        BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC;
    services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    services[0].uuid = &service_uuid.u;
    services[0].characteristics = characteristics;

    if (ble_gatts_count_cfg(services) != 0 || ble_gatts_add_svcs(services) != 0 ||
        ble_svc_gap_device_name_set(config_.device_name) != 0 ||
        ble_att_set_preferred_mtu(config_.preferred_mtu) != 0) {
        instance_ = nullptr;
        return Status::failure(ErrorCode::InternalFailure);
    }

    nimble_port_freertos_init(host_task);
    return Status::success();
}

bool EspBleAdapter::receive(BleFrame& frame) {
    RxItem item{};
    if (queue_ == nullptr || xQueueReceive(queue_, &item, 0) != pdTRUE) {
        return false;
    }
    frame.clear();
    return frame.append(item.data.data(), item.size);
}

Status EspBleAdapter::send(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0 || size > kMaximumBleFrameSize) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    if (!connected_.load() || !secure_.load() || !bonded_.load()) {
        return Status::failure(ErrorCode::InvalidState);
    }
    os_mbuf* packet = ble_hs_mbuf_from_flat(data, size);
    if (packet == nullptr) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    const int result = ble_gatts_notify_custom(
        connection_handle_.load(), response_value_handle_, packet);
    return result == 0 ? Status::success() : Status::failure(ErrorCode::ProtocolFailure);
}

bool EspBleAdapter::connected() const {
    return connected_.load();
}

bool EspBleAdapter::secure() const {
    return secure_.load();
}

bool EspBleAdapter::bonded() const {
    return bonded_.load();
}

Status EspBleAdapter::forget_bonds() {
    // 该入口只由加密 GATT 命令调用。清除 NVS 中的 LTK、IRK 和 CCCD；composition
    // root 会先发送响应再重启，使下一次连接必须重新配对。
    return ble_store_clear() == 0 ? Status::success()
                                  : Status::failure(ErrorCode::StorageFailure);
}

Status EspBleAdapter::request_fast_advertising() {
    if (connected_.load()) {
        return Status::failure(ErrorCode::Busy);
    }
    if (ble_gap_adv_active() == 0) {
        return start_advertising(AdvertisingMode::Fast);
    }
    // 停止广播会异步产生 ADV_COMPLETE；由该回调重启快速阶段，避免同时启动两次广播。
    fast_restart_pending_.store(true);
    if (ble_gap_adv_stop() != 0) {
        fast_restart_pending_.store(false);
        return Status::failure(ErrorCode::InternalFailure);
    }
    return Status::success();
}

int EspBleAdapter::gap_event(ble_gap_event* event, void* argument) {
    auto* self = static_cast<EspBleAdapter*>(argument);
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(kTag, "BLE peer connected handle=%u",
                         event->connect.conn_handle);
                self->connection_handle_.store(event->connect.conn_handle);
                self->connected_.store(true);
                self->secure_.store(false);
                self->bonded_.store(false);
                self->signal_runtime_event();
                if (ble_gap_security_initiate(event->connect.conn_handle) != 0) {
                    (void)ble_gap_terminate(
                        event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                }
            } else {
                ESP_LOGW(kTag, "BLE connection attempt failed status=%d",
                         event->connect.status);
                (void)self->start_advertising(AdvertisingMode::Fast);
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(kTag, "BLE peer disconnected reason=%d",
                     event->disconnect.reason);
            self->connected_.store(false);
            self->secure_.store(false);
            self->bonded_.store(false);
            self->connection_handle_.store(BLE_HS_CONN_HANDLE_NONE);
            self->signal_runtime_event();
            (void)self->start_advertising(AdvertisingMode::Fast);
            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            (void)self->start_advertising(
                self->fast_restart_pending_.exchange(false)
                    ? AdvertisingMode::Fast
                    : AdvertisingMode::Slow);
            return 0;
        case BLE_GAP_EVENT_ENC_CHANGE: {
            ESP_LOGI(kTag, "BLE encryption change handle=%u status=%d",
                     event->enc_change.conn_handle, event->enc_change.status);
            ble_gap_conn_desc descriptor{};
            if (event->enc_change.status == 0 &&
                ble_gap_conn_find(event->enc_change.conn_handle, &descriptor) == 0) {
                ESP_LOGI(kTag,
                         "BLE security encrypted=%u authenticated=%u bonded=%u key_size=%u",
                         descriptor.sec_state.encrypted,
                         descriptor.sec_state.authenticated,
                         descriptor.sec_state.bonded,
                         descriptor.sec_state.key_size);
                self->secure_.store(descriptor.sec_state.encrypted != 0);
                self->bonded_.store(descriptor.sec_state.bonded != 0);
            } else {
                self->secure_.store(false);
                self->bonded_.store(false);
            }
            self->signal_runtime_event();
            return 0;
        }
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(kTag, "BLE subscription handle=%u notify=%u indicate=%u",
                     event->subscribe.attr_handle,
                     event->subscribe.cur_notify,
                     event->subscribe.cur_indicate);
            return 0;
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            // 不自动删除已持久化的绑定；解绑必须走显式的本机恢复流程。
            ESP_LOGW(kTag, "BLE peer requested repeat pairing; preserving existing bond");
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        default:
            return 0;
    }
}

int EspBleAdapter::gatt_access(
    std::uint16_t,
    std::uint16_t,
    ble_gatt_access_ctxt* context,
    void*) {
    if (instance_ == nullptr || context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGW(kTag, "BLE GATT access rejected op=%u", context->op);
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (!instance_->connected_.load() || !instance_->secure_.load() ||
        !instance_->bonded_.load()) {
        ESP_LOGW(kTag, "BLE command rejected connected=%u secure=%u bonded=%u",
                 instance_->connected_.load(),
                 instance_->secure_.load(),
                 instance_->bonded_.load());
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    }
    const std::size_t size = OS_MBUF_PKTLEN(context->om);
    if (size == 0 || size > kMaximumBleFrameSize) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    std::array<std::uint8_t, kMaximumBleFrameSize> data{};
    if (os_mbuf_copydata(context->om, 0, size, data.data()) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return instance_->enqueue(data.data(), size) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int EspBleAdapter::store_status(ble_store_status_event* event, void*) {
    if (event != nullptr) {
        ESP_LOGW(kTag, "BLE persistent store full/event=%d; preserving existing bonds",
                 event->event_code);
    }
    return BLE_HS_ENOMEM;
}

void EspBleAdapter::on_sync() {
    if (instance_ == nullptr) {
        return;
    }
    if (ble_hs_util_ensure_addr(0) != 0) {
        ESP_LOGE(kTag, "failed to establish BLE identity");
        return;
    }
    (void)instance_->start_advertising(AdvertisingMode::Fast);
}

void EspBleAdapter::on_reset(int reason) {
    ESP_LOGE(kTag, "NimBLE reset reason=%d", reason);
    if (instance_ != nullptr) {
        instance_->connected_.store(false);
        instance_->secure_.store(false);
        instance_->bonded_.store(false);
        instance_->signal_runtime_event();
    }
}

void EspBleAdapter::host_task(void*) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

Status EspBleAdapter::start_advertising(AdvertisingMode mode) {
    std::uint8_t address_type = 0;
    if (ble_hs_id_infer_auto(0, &address_type) != 0) {
        return Status::failure(ErrorCode::InternalFailure);
    }

    ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = reinterpret_cast<std::uint8_t*>(
        const_cast<char*>(config_.device_name));
    fields.name_len = std::strlen(config_.device_name);
    fields.name_is_complete = 1;
    fields.uuids16 = &service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    if (ble_gap_adv_set_fields(&fields) != 0) {
        return Status::failure(ErrorCode::InternalFailure);
    }

    ble_gap_adv_params parameters{};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    const bool fast = mode == AdvertisingMode::Fast;
    parameters.itvl_min = fast ? config_.fast_advertising_interval_min_units
                               : config_.slow_advertising_interval_min_units;
    parameters.itvl_max = fast ? config_.fast_advertising_interval_max_units
                               : config_.slow_advertising_interval_max_units;
    const std::int32_t duration_ms =
        fast && config_.fast_advertising_duration_ms != 0
            ? static_cast<std::int32_t>(config_.fast_advertising_duration_ms)
            : BLE_HS_FOREVER;
    return ble_gap_adv_start(
               address_type,
               nullptr,
               duration_ms,
               &parameters,
               gap_event,
               this) == 0
               ? Status::success()
               : Status::failure(ErrorCode::InternalFailure);
}

bool EspBleAdapter::enqueue(const std::uint8_t* data, std::size_t size) {
    if (queue_ == nullptr || size > kMaximumBleFrameSize) {
        return false;
    }
    RxItem item{};
    item.size = static_cast<std::uint16_t>(size);
    for (std::size_t index = 0; index < size; ++index) {
        item.data[index] = data[index];
    }
    if (xQueueSend(queue_, &item, 0) != pdTRUE) {
        return false;
    }
    signal_runtime_event();
    return true;
}

void EspBleAdapter::signal_runtime_event() const noexcept {
    config_.runtime_event.notify(false);
}

}  // namespace plant
