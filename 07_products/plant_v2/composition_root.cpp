#include "07_products/plant_v2/composition_root.hpp"

#include <cinttypes>
#include <cstdlib>

#include "03_services/communication/communication_service.hpp"
#include "04_protocol/messages/protocol.hpp"
#include "05_adapters/espidf/ble/esp_ble_adapter.hpp"
#include "05_adapters/espidf/haptic/vibration_adapter.hpp"
#include "05_adapters/espidf/light/led_adapter.hpp"
#include "05_adapters/espidf/motion/feedback_servo_adapter.hpp"
#include "05_adapters/espidf/ota/esp_ota_adapter.hpp"
#include "05_adapters/espidf/power/esp_power_adapter.hpp"
#include "05_adapters/espidf/sensing/aht21_adapter.hpp"
#include "05_adapters/espidf/sensing/max17048_adapter.hpp"
#include "05_adapters/espidf/sensing/v2_acoustic_adapter.hpp"
#include "05_adapters/espidf/sensing/v2_illumination_adapter.hpp"
#include "05_adapters/espidf/touch/touch_adapter.hpp"
#include "06_bsp/plant_v2/plant_v2_board.hpp"
#include "07_products/plant_v1/app/plant_application.hpp"
#include "07_products/plant_v2/app/plant_v2_application.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace plant::product::v2 {
namespace {

using Config = bsp::v2::BoardConfig;
constexpr char kTag[] = "plant_v2";
constexpr std::uint64_t kBootSensorTimeoutUs = 5ULL * 1000ULL * 1000ULL;
constexpr std::uint64_t kBootConfirmationDelayUs = 10ULL * 1000ULL * 1000ULL;

V2AdcSampler adc;
V2I2cBus i2c;
FeedbackServoAdapter motion{adc};
EspLedAdapter led_output{EspLedConfig{
    Config::Gpio::led_red_pwm,
    Config::Gpio::led_green_pwm,
    Config::Gpio::led_blue_pwm,
    Config::Led::frequency_hz,
    Config::Led::duty_resolution_bits,
    Config::Led::maximum_duty,
    Config::Led::active_high,
}};
LightArbitrationService light{led_output};
EspVibrationAdapter haptic{EspVibrationConfig{
    Config::Gpio::vibration_pwm,
    Config::Vibration::frequency_hz,
    Config::Vibration::duty_resolution_bits,
    Config::Vibration::soft_duty,
    Config::Vibration::warning_duty,
    Config::Vibration::maximum_continuous_time_ms,
    Config::Vibration::active_high,
}};
EspTouchAdapter touch{EspTouchConfig{
    Config::Gpio::touch_input,
    Config::Touch::active_high,
    Config::Touch::debounce_ms,
    Config::Touch::long_press_ms,
    Config::Touch::factory_reset_hold_ms,
    Config::Touch::enable_internal_pull_down,
}};
EspBleAdapter ble{EspBleConfig{
    Config::Product::device_name,
    Config::Ble::service_uuid,
    Config::Ble::command_uuid,
    Config::Ble::response_uuid,
    Config::Ble::preferred_mtu,
    Config::Ble::receive_queue_depth,
    Config::Ble::advertising_interval_min_units,
    Config::Ble::advertising_interval_max_units,
}};
EspPowerAdapter power_port{EspPowerConfig{
    Config::Gpio::touch_input,
    Config::Touch::active_high,
    Config::Power::maximum_cpu_frequency_mhz,
    Config::Power::minimum_cpu_frequency_mhz,
}};
EspOtaAdapter ota_port;
V2AcousticAdapter acoustic_port{adc};
V2IlluminationAdapter illumination_port{adc};
Aht21Adapter climate_port{i2c};
Max17048Adapter battery_port{i2c};

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
PlantApplication base_application{behavior, lifecycle, power, ota};
AcousticService acoustic{AcousticDetectionConfig{
    Config::Microphone::initial_noise_floor,
    Config::Microphone::speech_start_margin,
    Config::Microphone::speech_stop_margin,
    Config::Microphone::minimum_speech_ms * 1000ULL,
    Config::Microphone::speech_end_hold_ms * 1000ULL,
    Config::Microphone::sustained_window_ms * 1000ULL,
    Config::Microphone::sustained_required_ms * 1000ULL,
}};
IlluminationService illumination{IlluminationDetectionConfig{
    Config::Illumination::dark_threshold,
    Config::Illumination::bright_enter_threshold,
    Config::Illumination::bright_exit_threshold,
    Config::Illumination::bright_confirm_ms * 1000ULL,
    Config::Illumination::bright_exit_hold_ms * 1000ULL,
    Config::Illumination::exposure_credit_interval_ms * 1000ULL,
}};
ClimateService climate{ClimateDetectionConfig{
    Config::Climate::suitable_min_temperature_centi_c,
    Config::Climate::suitable_max_temperature_centi_c,
    Config::Climate::suitable_min_humidity_tenths_percent,
    Config::Climate::suitable_max_humidity_tenths_percent,
    Config::Climate::temperature_hysteresis_centi_c,
    Config::Climate::humidity_hysteresis_tenths_percent,
    Config::Climate::suitable_credit_interval_ms * 1000ULL,
}};
BatteryService battery{
    Config::Battery::low_level_per_mille,
    Config::Battery::critical_level_per_mille,
};
GrowthService growth{GrowthConfig{
    Config::Growth::step,
    Config::Growth::maximum_position,
    Config::Growth::limit_tolerance,
    Config::Growth::pending_expiry_ms * 1000ULL,
    {
        0,
        Config::Growth::touch_cooldown_ms * 1000ULL,
        Config::Growth::speech_cooldown_ms * 1000ULL,
        Config::Growth::sunlight_cooldown_ms * 1000ULL,
        Config::Growth::climate_cooldown_ms * 1000ULL,
    },
}};
PlantV2Application application{
    base_application,
    lifecycle,
    behavior,
    ota,
    motion,
    acoustic_port,
    illumination_port,
    climate_port,
    battery_port,
    acoustic,
    illumination,
    climate,
    battery,
    growth,
    light,
};
CommunicationService communication{ble};

std::uint64_t boot_started_us = 0;
std::uint64_t last_activity_us = 0;
bool hardware_configuration_ok = false;
bool boot_finalized = false;
bool communication_was_connected = false;
bool communication_was_secure = false;

static_assert(Config::Product::ota_chunk_size == kMaximumOtaChunkSize);

bool initialize_hardware() {
    bool ok = true;
    ok = adc.initialize().ok() && ok;
    ok = i2c.initialize().ok() && ok;
    ok = motion.initialize().ok() && ok;
    ok = led_output.initialize().ok() && ok;
    ok = haptic.initialize().ok() && ok;
    ok = touch.initialize().ok() && ok;
    ok = power_port.initialize().ok() && ok;
    ok = acoustic_port.initialize().ok() && ok;
    ok = illumination_port.initialize().ok() && ok;
    ok = climate_port.initialize().ok() && ok;
    ok = battery_port.initialize().ok() && ok;
    ok = communication.initialize().ok() && ok;
    return ok;
}

ErrorCode active_fault() {
    if (lifecycle.snapshot().state != DeviceState::Fault) {
        return ErrorCode::None;
    }
    if (motion.position_snapshot().fault != MotionFault::None ||
        motion.position_snapshot().feedback != PositionFeedbackState::Valid) {
        return ErrorCode::MotionFailure;
    }
    if (acoustic.snapshot().state == AcousticState::SensorFault ||
        illumination.snapshot().state == IlluminationState::SensorFault ||
        climate.snapshot().state == ClimateState::SensorFault ||
        battery.snapshot().state == BatteryState::SensorFault) {
        return ErrorCode::SensorFailure;
    }
    return ErrorCode::InternalFailure;
}

void respond_with_current_state(const Command& command, Status status) {
    const LifecycleSnapshot lifecycle_state = base_application.lifecycle_snapshot();
    const BehaviorSnapshot behavior_state = base_application.behavior_snapshot();
    const OtaSnapshot ota_state = base_application.ota_snapshot();
    (void)communication.respond(
        command,
        status,
        CommunicationState{
            lifecycle_state.state,
            lifecycle_state.power_mode,
            application.growth_motion_active() ? Behavior::Grow : behavior_state.behavior,
            ota_state.state,
            static_cast<std::uint32_t>(ota_state.received_bytes),
            Config::Product::firmware_version,
            protocol::PositionFeedbackCapability |
                protocol::AcousticActivityCapability |
                protocol::RelativeIlluminationCapability |
                protocol::ClimateCapability |
                protocol::GrowthCapability |
                protocol::BatteryCapability |
                protocol::PersistentBondingCapability,
            application.position_snapshot(),
            application.acoustic_snapshot(),
            application.illumination_snapshot(),
            application.climate_snapshot(),
            application.battery_snapshot(),
            application.growth_snapshot(),
            active_fault(),
            ble.secure(),
            ble.bonded(),
        });
}

void finish_boot(bool sensors_ok, std::uint64_t now_us) {
    const Status ota_boot_status =
        sensors_ok ? Status::success() : ota.finalize_boot(false);
    const bool platform_ok = sensors_ok && ota_boot_status.ok();
    const Status application_status = base_application.finish_boot(platform_ok);
    const bool boot_ok = platform_ok && application_status.ok();
    if (!application_status.ok()) {
        (void)ota.finalize_boot(false);
    }
    if (boot_ok) {
        ota.schedule_boot_confirmation(now_us, kBootConfirmationDelayUs);
    }
    boot_finalized = true;
    last_activity_us = now_us;
    ESP_LOGI(
        kTag,
        "boot product=%s hw=%" PRIu32 " firmware=0x%08" PRIx32 " status=%s",
        Config::Product::device_name,
        Config::Product::hardware_revision,
        Config::Product::firmware_version,
        boot_ok ? "ready" : "fault");
}

bool position_is_safe_for_deep_sleep() {
    const PositionSnapshot position = motion.position_snapshot();
    return position.feedback == PositionFeedbackState::Valid && !position.moving &&
           std::abs(static_cast<int>(position.actual_position) - Config::Position::sleep) <=
               Config::Position::tolerance;
}

}  // namespace

void initialize() {
    boot_started_us = static_cast<std::uint64_t>(esp_timer_get_time());
    last_activity_us = boot_started_us;
    hardware_configuration_ok = initialize_hardware();
    communication_was_connected = communication.connected();
    communication_was_secure = ble.secure();
    if (!hardware_configuration_ok) {
        finish_boot(false, boot_started_us);
    }
}

[[noreturn]] void run() {
    while (true) {
        const std::uint64_t now_us = static_cast<std::uint64_t>(esp_timer_get_time());
        if (!boot_finalized) {
            (void)application.tick(now_us);
            if (application.boot_sensors_ready() ||
                now_us - boot_started_us >= kBootSensorTimeoutUs) {
                finish_boot(application.boot_sensors_ok(), now_us);
            }
            vTaskDelay(pdMS_TO_TICKS(Config::Interaction::system_tick_ms));
            continue;
        }

        const DeviceState lifecycle_before_tick = lifecycle.snapshot().state;
        (void)application.tick(now_us);

        bool confirmation_attempted = false;
        const Status confirmation_status = ota.poll_boot_confirmation(
            now_us,
            lifecycle.snapshot().state != DeviceState::Fault,
            confirmation_attempted);
        if (confirmation_attempted && !confirmation_status.ok()) {
            (void)base_application.handle_behavior_event(
                BehaviorEvent{BehaviorEventType::FaultRaised, 0});
        }

        const bool communication_connected = communication.connected();
        const bool communication_secure = ble.secure();
        if (!communication_was_secure && communication_secure) {
            last_activity_us = now_us;
            (void)application.handle_communication_connected();
        }
        if (communication_was_connected && !communication_connected) {
            (void)application.handle_communication_disconnected();
        }
        communication_was_connected = communication_connected;
        communication_was_secure = communication_secure;

        TouchGesture gesture{};
        if (touch.poll(now_us / 1000ULL, gesture)) {
            last_activity_us = now_us;
            if (gesture == TouchGesture::FactoryResetHold) {
                const Status reset_status = ble.forget_bonds();
                if (reset_status.ok()) {
                    ESP_LOGW(kTag, "local recovery cleared all BLE bonds");
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                }
                ESP_LOGE(kTag, "failed to clear BLE bonds");
            } else {
                (void)application.handle_touch(gesture, now_us);
            }
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
                const Status execution_status = command.type == CommandType::ForgetBonds
                                                    ? ble.forget_bonds()
                                                    : application.handle_command(command);
                respond_with_current_state(command, execution_status);
                if (execution_status.ok() && command.type == CommandType::FinishOta) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                }
                if (execution_status.ok() && command.type == CommandType::ForgetBonds) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                }
            }
        }

        const DeviceState lifecycle_after_tick = lifecycle.snapshot().state;
        if ((lifecycle_after_tick == DeviceState::Idle ||
             lifecycle_after_tick == DeviceState::Sleeping) &&
            lifecycle_before_tick == DeviceState::Interacting &&
            !application.growth_motion_active()) {
            last_activity_us = static_cast<std::uint64_t>(esp_timer_get_time());
        }

        const LifecycleSnapshot lifecycle_state = lifecycle.snapshot();
        if (lifecycle_state.state == DeviceState::Idle &&
            !application.growth_motion_active() &&
            now_us - last_activity_us >=
                Config::Interaction::automatic_sleep_ms * 1000ULL) {
            last_activity_us = now_us;
            (void)application.handle_idle_timeout();
        }
        if (lifecycle_state.state == DeviceState::Sleeping &&
            lifecycle_state.power_mode == PowerMode::LightSleep &&
            Config::Power::deep_sleep_enabled &&
            Config::Power::deep_sleep_delay_ms != 0 &&
            position_is_safe_for_deep_sleep() &&
            now_us - last_activity_us >= Config::Power::deep_sleep_delay_ms * 1000ULL) {
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

}  // namespace plant::product::v2
