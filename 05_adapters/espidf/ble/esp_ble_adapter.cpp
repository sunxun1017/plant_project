#include "05_adapters/espidf/ble/esp_ble_adapter.hpp"

#include <cstring>

#include "esp_log.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
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

using Config = bsp::v1::BoardConfig;
constexpr char kTag[] = "plant_ble";
ble_uuid16_t service_uuid = BLE_UUID16_INIT(Config::Ble::service_uuid);
ble_uuid16_t command_uuid = BLE_UUID16_INIT(Config::Ble::command_uuid);
ble_uuid16_t response_uuid = BLE_UUID16_INIT(Config::Ble::response_uuid);

ble_gatt_chr_def characteristics[3]{};
ble_gatt_svc_def services[2]{};

}  // namespace

EspBleAdapter* EspBleAdapter::instance_ = nullptr;
std::uint16_t EspBleAdapter::response_value_handle_ = 0;

Status EspBleAdapter::initialize() {
    if (instance_ != nullptr) {
        return Status::failure(ErrorCode::InvalidState);
    }
    queue_ = xQueueCreateStatic(
        Config::Ble::receive_queue_depth,
        sizeof(RxItem),
        queue_storage_.data(),
        &queue_control_);
    if (queue_ == nullptr) {
        return Status::failure(ErrorCode::InternalFailure);
    }
    instance_ = this;

    esp_err_t nvs_status = nvs_flash_init();
    if (nvs_status == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_status == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_status = nvs_flash_erase();
        if (nvs_status == ESP_OK) {
            nvs_status = nvs_flash_init();
        }
    }
    if (nvs_status != ESP_OK || nimble_port_init() != ESP_OK) {
        instance_ = nullptr;
        return Status::failure(ErrorCode::InternalFailure);
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();

    characteristics[0].uuid = &command_uuid.u;
    characteristics[0].access_cb = gatt_access;
    characteristics[0].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP;
    characteristics[1].uuid = &response_uuid.u;
    characteristics[1].val_handle = &response_value_handle_;
    characteristics[1].flags = BLE_GATT_CHR_F_NOTIFY;
    services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    services[0].uuid = &service_uuid.u;
    services[0].characteristics = characteristics;

    if (ble_gatts_count_cfg(services) != 0 || ble_gatts_add_svcs(services) != 0 ||
        ble_svc_gap_device_name_set(Config::Product::device_name) != 0 ||
        ble_att_set_preferred_mtu(Config::Ble::preferred_mtu) != 0) {
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
    if (!connected_.load()) {
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

int EspBleAdapter::gap_event(ble_gap_event* event, void* argument) {
    auto* self = static_cast<EspBleAdapter*>(argument);
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                self->connection_handle_.store(event->connect.conn_handle);
                self->connected_.store(true);
            } else {
                (void)self->start_advertising();
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            self->connected_.store(false);
            self->connection_handle_.store(BLE_HS_CONN_HANDLE_NONE);
            (void)self->start_advertising();
            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            (void)self->start_advertising();
            return 0;
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
        return BLE_ATT_ERR_UNLIKELY;
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

void EspBleAdapter::on_sync() {
    if (instance_ == nullptr) {
        return;
    }
    if (ble_hs_util_ensure_addr(0) != 0) {
        ESP_LOGE(kTag, "failed to establish BLE identity");
        return;
    }
    (void)instance_->start_advertising();
}

void EspBleAdapter::on_reset(int reason) {
    ESP_LOGE(kTag, "NimBLE reset reason=%d", reason);
    if (instance_ != nullptr) {
        instance_->connected_.store(false);
    }
}

void EspBleAdapter::host_task(void*) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

Status EspBleAdapter::start_advertising() {
    std::uint8_t address_type = 0;
    if (ble_hs_id_infer_auto(0, &address_type) != 0) {
        return Status::failure(ErrorCode::InternalFailure);
    }

    ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = reinterpret_cast<std::uint8_t*>(
        const_cast<char*>(Config::Product::device_name));
    fields.name_len = std::strlen(Config::Product::device_name);
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
    return ble_gap_adv_start(
               address_type,
               nullptr,
               BLE_HS_FOREVER,
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
    return xQueueSend(queue_, &item, 0) == pdTRUE;
}

}  // namespace plant
