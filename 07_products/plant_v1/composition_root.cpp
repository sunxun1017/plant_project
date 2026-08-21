#include "07_products/plant_v1/composition_root.hpp"

#include <cinttypes>
#include <cstdint>

#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "07_products/plant_v1/app/plant_application.hpp"
#include "05_adapters/espidf/haptic/vibration_adapter.hpp"
#include "05_adapters/espidf/ble/esp_ble_adapter.hpp"
#include "05_adapters/espidf/light/led_adapter.hpp"
#include "05_adapters/espidf/motion/servo_adapter.hpp"
#include "05_adapters/espidf/ota/esp_ota_adapter.hpp"
#include "05_adapters/espidf/power/esp_power_adapter.hpp"
#include "05_adapters/espidf/touch/touch_adapter.hpp"
#include "03_services/communication/communication_service.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

namespace plant::product::v1 {
namespace {

using Config = bsp::v1::BoardConfig;
constexpr char kTag[] = "plant_v1";
constexpr std::uint64_t kBootConfirmationDelayUs = 10ULL * 1000ULL * 1000ULL;

EspServoAdapter motion;
EspLedAdapter light;
EspVibrationAdapter haptic;
EspTouchAdapter touch;
EspBleAdapter ble;
EspPowerAdapter power_port;
EspOtaAdapter ota_port;

BehaviorService behavior{motion, light, haptic};
LifecycleService lifecycle;
PowerService power{
    lifecycle,
    power_port,
    LowPowerConfig{
        Config::Power::deep_sleep_enabled,
        Config::Power::deep_sleep_delay_ms,
        Config::Power::timer_wakeup_us,
    }};
OtaService ota{
    lifecycle,
    ota_port,
    OtaProductIdentity{
        Config::Product::product_id,
        Config::Product::hardware_revision,
        Config::Product::firmware_version,
    }};
PlantApplication application{behavior, lifecycle, power, ota};
CommunicationService communication{ble};
std::uint64_t last_activity_us = 0;
std::uint64_t boot_confirmation_due_us = 0;
bool boot_confirmation_pending = false;
bool communication_was_connected = false;

static_assert(Config::Product::ota_chunk_size == kMaximumOtaChunkSize);

bool initialize_hardware() {
    bool ok = true;
    ok = motion.initialize().ok() && ok;
    ok = light.initialize().ok() && ok;
    ok = haptic.initialize().ok() && ok;
    ok = touch.initialize().ok() && ok;
    ok = power_port.initialize().ok() && ok;
    ok = communication.initialize().ok() && ok;
    return ok;
}

void respond_with_current_state(const Command& command, Status status) {
    const auto lifecycle_state = application.lifecycle_snapshot();
    const auto behavior_state = application.behavior_snapshot();
    const auto ota_state = application.ota_snapshot();
    (void)communication.respond(
        command,
        status,
        CommunicationState{
            lifecycle_state.state,
            lifecycle_state.power_mode,
            behavior_state.behavior,
            ota_state.state,
            static_cast<std::uint32_t>(ota_state.received_bytes),
            Config::Product::firmware_version,
        });
}

}  // namespace

void initialize() {
    const bool hardware_ok = initialize_hardware();
    const Status ota_boot_status =
        hardware_ok ? Status::success() : ota_port.finalize_boot(false);
    const bool boot_ok = hardware_ok && ota_boot_status.ok();
    (void)application.finish_boot(boot_ok);
    last_activity_us = static_cast<std::uint64_t>(esp_timer_get_time());
    boot_confirmation_due_us = last_activity_us + kBootConfirmationDelayUs;
    boot_confirmation_pending = boot_ok;
    communication_was_connected = communication.connected();
    ESP_LOGI(
        kTag,
        "boot product=%s hw=%" PRIu32 " firmware=0x%08" PRIx32 " status=%s",
        Config::Product::device_name,
        Config::Product::hardware_revision,
        Config::Product::firmware_version,
        boot_ok ? "ready" : "fault");
}

[[noreturn]] void run() {
    while (true) {
        const std::uint64_t now_us = static_cast<std::uint64_t>(esp_timer_get_time());
        const DeviceState lifecycle_state_before_tick = lifecycle.snapshot().state;
        (void)application.tick(now_us);

        if (boot_confirmation_pending && now_us >= boot_confirmation_due_us) {
            const bool self_test_ok =
                lifecycle.snapshot().state != DeviceState::Fault;
            const Status confirmation_status = ota_port.finalize_boot(self_test_ok);
            boot_confirmation_pending = false;
            if (!confirmation_status.ok()) {
                (void)application.handle_behavior_event(
                    BehaviorEvent{BehaviorEventType::FaultRaised, 0});
            }
        }

        const bool communication_connected = communication.connected();
        if (!communication_was_connected && communication_connected) {
            last_activity_us = now_us;
            (void)application.handle_communication_connected();
        }
        if (communication_was_connected && !communication_connected) {
            (void)application.handle_communication_disconnected();
        }
        communication_was_connected = communication_connected;

        TouchGesture gesture{};
        if (touch.poll(now_us / 1000ULL, gesture)) {
            last_activity_us = now_us;
            (void)application.handle_touch(gesture);
        }

        Command command{};
        bool command_available = false;
        const Status receive_status = communication.poll(command, command_available);
        if (command_available) {
            if (!receive_status.ok()) {
                if (receive_status.code() != ErrorCode::ProtocolFailure) {
                    respond_with_current_state(command, receive_status);
                }
            } else {
                last_activity_us = now_us;
                const Status execution_status = application.handle_command(command);
                respond_with_current_state(command, execution_status);
                if (execution_status.ok() && command.type == CommandType::FinishOta) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                }
            }
        }

        const DeviceState state_after_tick = lifecycle.snapshot().state;
        if (state_after_tick == DeviceState::Idle ||
            state_after_tick == DeviceState::Sleeping) {
            const BehaviorRunState behavior_state = behavior.snapshot().state;
            if (behavior_state == BehaviorRunState::Idle &&
                lifecycle_state_before_tick == DeviceState::Interacting) {
                last_activity_us = static_cast<std::uint64_t>(esp_timer_get_time());
            }
        }

        const LifecycleSnapshot lifecycle_state = lifecycle.snapshot();
        if (lifecycle_state.state == DeviceState::Idle &&
            now_us - last_activity_us >=
                static_cast<std::uint64_t>(Config::Interaction::automatic_sleep_ms) * 1000ULL) {
            last_activity_us = now_us;
            (void)application.handle_idle_timeout();
        }
        if (lifecycle_state.state == DeviceState::Sleeping &&
            lifecycle_state.power_mode == PowerMode::LightSleep &&
            Config::Power::deep_sleep_enabled && Config::Power::deep_sleep_delay_ms != 0 &&
            now_us - last_activity_us >=
                static_cast<std::uint64_t>(Config::Power::deep_sleep_delay_ms) * 1000ULL) {
            const OtaState ota_state = ota.snapshot().state;
            (void)power.request_deep_sleep(PowerConditions{
                communication.connected(),
                ota_state == OtaState::Receiving || ota_state == OtaState::Verifying,
                false,
            });
        }
        vTaskDelay(pdMS_TO_TICKS(Config::Interaction::system_tick_ms));
    }
}

}  // namespace plant::product::v1
