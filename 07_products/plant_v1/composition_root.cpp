#include "07_products/plant_v1/composition_root.hpp"

#include <cinttypes>
#include <cstdint>

#include "06_bsp/plant_v1/plant_v1_board.hpp"
#include "07_products/plant_v1/app/plant_application.hpp"
#include "05_adapters/espidf/haptic/vibration_adapter.hpp"
#include "05_adapters/espidf/light/led_adapter.hpp"
#include "05_adapters/espidf/motion/servo_adapter.hpp"
#include "05_adapters/espidf/ota/esp_ota_adapter.hpp"
#include "05_adapters/espidf/power/esp_power_adapter.hpp"
#include "05_adapters/espidf/touch/touch_adapter.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace plant::product::v1 {
namespace {

using Config = bsp::v1::BoardConfig;
constexpr char kTag[] = "plant_v1";

EspServoAdapter motion;
EspLedAdapter light;
EspVibrationAdapter haptic;
EspTouchAdapter touch;
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
std::uint64_t last_activity_us = 0;

bool initialize_hardware() {
    bool ok = true;
    ok = motion.initialize().ok() && ok;
    ok = light.initialize().ok() && ok;
    ok = haptic.initialize().ok() && ok;
    ok = touch.initialize().ok() && ok;
    return ok;
}

void handle_return_from_light_sleep() {
    const LifecycleSnapshot snapshot = lifecycle.snapshot();
    if (snapshot.state != DeviceState::Sleeping ||
        snapshot.power_mode != PowerMode::LightSleep) {
        return;
    }
    if (power_port.wake_source() == WakeSource::Timer &&
        Config::Power::deep_sleep_enabled) {
        (void)power.request_deep_sleep(PowerConditions{});
        return;
    }
    if (power.handle_wake().ok()) {
        last_activity_us = static_cast<std::uint64_t>(esp_timer_get_time());
        (void)application.request_behavior(Behavior::WakeUp, InterruptionReason::WakeSleep);
    }
}

}  // namespace

void initialize() {
    const bool hardware_ok = initialize_hardware();
    (void)application.finish_boot(hardware_ok);
    last_activity_us = static_cast<std::uint64_t>(esp_timer_get_time());
    ESP_LOGI(
        kTag,
        "boot product=%s hw=%" PRIu32 " firmware=0x%08" PRIx32 " status=%s",
        Config::Product::device_name,
        Config::Product::hardware_revision,
        Config::Product::firmware_version,
        hardware_ok ? "ready" : "fault");
}

[[noreturn]] void run() {
    while (true) {
        const std::uint64_t now_us = static_cast<std::uint64_t>(esp_timer_get_time());
        light.tick(now_us);
        haptic.tick(now_us);

        TouchGesture gesture{};
        if (touch.poll(now_us / 1000ULL, gesture)) {
            last_activity_us = now_us;
            (void)application.handle_touch(gesture);
        }

        std::uint32_t execution_id = 0;
        if (motion.poll(now_us, execution_id)) {
            (void)application.handle_behavior_event(
                BehaviorEvent{BehaviorEventType::MotionCompleted, execution_id});
            if (lifecycle.snapshot().state == DeviceState::Idle) {
                last_activity_us = static_cast<std::uint64_t>(esp_timer_get_time());
            }
            handle_return_from_light_sleep();
        }

        if (lifecycle.snapshot().state == DeviceState::Idle &&
            now_us - last_activity_us >=
                static_cast<std::uint64_t>(Config::Interaction::automatic_sleep_ms) * 1000ULL) {
            last_activity_us = now_us;
            (void)application.handle_idle_timeout();
        }
        vTaskDelay(pdMS_TO_TICKS(Config::Interaction::system_tick_ms));
    }
}

}  // namespace plant::product::v1
