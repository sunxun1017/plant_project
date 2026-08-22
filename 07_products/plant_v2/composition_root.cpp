#include "07_products/plant_v2/composition_root.hpp"

#include <algorithm>
#include <cinttypes>

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
#include "07_products/plant_v2/runtime_schedule.hpp"
#include "10_config/plant_v2/plant_v2_product_config.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace plant::product::v2 {
namespace {

using Board = bsp::v2::BoardConfig;
using Product = config::v2::ProductConfig;
constexpr char kTag[] = "plant_v2";
constexpr std::uint64_t kBootSensorTimeoutUs = 5ULL * 1000ULL * 1000ULL;
constexpr std::uint64_t kBootConfirmationDelayUs = 10ULL * 1000ULL * 1000ULL;
TaskHandle_t system_task_handle = nullptr;

void signal_system_task(void*, bool from_isr) noexcept {
    if (system_task_handle == nullptr) {
        return;
    }
    if (from_isr) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        vTaskNotifyGiveFromISR(system_task_handle, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
        return;
    }
    xTaskNotifyGive(system_task_handle);
}

V2AdcSampler adc;
V2I2cBus i2c;
FeedbackServoAdapter motion{adc};
EspLedAdapter led_output{EspLedConfig{
    Board::Gpio::led_red_pwm,
    Board::Gpio::led_green_pwm,
    Board::Gpio::led_blue_pwm,
    Board::Led::frequency_hz,
    Board::Led::duty_resolution_bits,
    Board::Led::maximum_duty,
    Board::Led::active_high,
}};
LightArbitrationService light{led_output};
EspVibrationAdapter haptic{EspVibrationConfig{
    Board::Gpio::vibration_pwm,
    Board::Vibration::frequency_hz,
    Board::Vibration::duty_resolution_bits,
    Board::Vibration::soft_duty,
    Board::Vibration::warning_duty,
    Board::Vibration::maximum_continuous_time_ms,
    Board::Vibration::active_high,
}};
EspTouchAdapter touch{EspTouchConfig{
    Board::Gpio::touch_input,
    Board::Touch::active_high,
    Product::Touch::debounce_ms,
    Product::Touch::long_press_ms,
    Product::Touch::factory_reset_hold_ms,
    Board::Touch::enable_internal_pull_down,
    EspRuntimeEventSignal{signal_system_task, nullptr},
}};
EspBleAdapter ble{EspBleConfig{
    Product::Product::device_name,
    Product::Ble::service_uuid,
    Product::Ble::command_uuid,
    Product::Ble::response_uuid,
    Product::Ble::preferred_mtu,
    Product::Ble::receive_queue_depth,
    Product::Ble::fast_advertising_interval_min_units,
    Product::Ble::fast_advertising_interval_max_units,
    Product::Ble::fast_advertising_duration_ms,
    Product::Ble::slow_advertising_interval_min_units,
    Product::Ble::slow_advertising_interval_max_units,
    EspRuntimeEventSignal{signal_system_task, nullptr},
}};
EspPowerAdapter power_port{EspPowerConfig{
    Board::Gpio::touch_input,
    Board::Touch::active_high,
    Board::Power::maximum_cpu_frequency_mhz,
    Board::Power::minimum_cpu_frequency_mhz,
}};
EspOtaAdapter ota_port;
V2AcousticAdapter acoustic_port{adc};
V2IlluminationAdapter illumination_port{adc};
Aht21Adapter climate_port{i2c};
Max17048Adapter battery_port{i2c};

BehaviorService behavior{
    motion,
    light,
    haptic,
    BehaviorExecutionConfig{
        5ULL * 1000ULL * 1000ULL,
        Product::Behavior::expressive_motion_enabled,
    }};
LifecycleService lifecycle;
PowerService power{
    lifecycle,
    power_port,
    LowPowerConfig{
        Product::Power::deep_sleep_enabled,
        Product::Power::deep_sleep_delay_ms,
        Product::Power::timer_wakeup_us,
    }};
OtaService ota{
    lifecycle,
    ota_port,
    OtaProductIdentity{
        Product::Product::product_id,
        Product::Product::hardware_revision,
        Product::Product::firmware_version,
    }};
PlantApplication base_application{behavior, lifecycle, power, ota};
AcousticService acoustic{AcousticDetectionConfig{
    Product::Acoustic::initial_noise_floor,
    Product::Acoustic::speech_start_margin,
    Product::Acoustic::speech_stop_margin,
    Product::Acoustic::minimum_speech_ms * 1000ULL,
    Product::Acoustic::speech_end_hold_ms * 1000ULL,
    Product::Acoustic::sustained_window_ms * 1000ULL,
    Product::Acoustic::sustained_required_ms * 1000ULL,
}};
IlluminationService illumination{IlluminationDetectionConfig{
    Product::Illumination::dark_threshold,
    Product::Illumination::bright_enter_threshold,
    Product::Illumination::bright_exit_threshold,
    Product::Illumination::bright_confirm_ms * 1000ULL,
    Product::Illumination::bright_exit_hold_ms * 1000ULL,
    Product::Illumination::exposure_credit_interval_ms * 1000ULL,
}};
ClimateService climate{ClimateDetectionConfig{
    Product::Climate::suitable_min_temperature_centi_c,
    Product::Climate::suitable_max_temperature_centi_c,
    Product::Climate::suitable_min_humidity_tenths_percent,
    Product::Climate::suitable_max_humidity_tenths_percent,
    Product::Climate::temperature_hysteresis_centi_c,
    Product::Climate::humidity_hysteresis_tenths_percent,
    Product::Climate::suitable_credit_interval_ms * 1000ULL,
}};
BatteryService battery{
    Product::Battery::low_level_per_mille,
    Product::Battery::critical_level_per_mille,
};
GrowthService growth{GrowthConfig{
    Product::Growth::step,
    Board::Position::safe_maximum,
    Board::Position::tolerance,
    Product::Growth::pending_expiry_ms * 1000ULL,
    {
        0,
        Product::Growth::touch_cooldown_ms * 1000ULL,
        Product::Growth::speech_cooldown_ms * 1000ULL,
        Product::Growth::sunlight_cooldown_ms * 1000ULL,
        Product::Growth::climate_cooldown_ms * 1000ULL,
    },
    Board::Position::safe_minimum,
    Product::Growth::decay_step,
    Product::Growth::inactivity_before_decay_ms * 1000ULL,
    Product::Growth::decay_interval_ms * 1000ULL,
    Product::Growth::maximum_pending_credits,
}};
PlantV2Application application{
    base_application,
    lifecycle,
    behavior,
    ota,
    motion,
    haptic,
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
    Product::Acoustic::actuator_recovery_ms * 1000ULL,
};
CommunicationService communication{ble};

std::uint64_t boot_started_us = 0;
std::uint64_t last_activity_us = 0;
bool hardware_configuration_ok = false;
bool boot_finalized = false;
bool communication_was_connected = false;
bool communication_was_secure = false;
ErrorCode last_tick_error = ErrorCode::None;

static_assert(Product::Product::ota_chunk_size == kMaximumOtaChunkSize);

void report_tick_status(Status status) {
    if (status.ok()) {
        last_tick_error = ErrorCode::None;
        return;
    }
    if (status.code() != last_tick_error) {
        ESP_LOGE(kTag, "application tick failed: error=%d", static_cast<int>(status.code()));
        last_tick_error = status.code();
    }
}

void report_event_failure(const char* operation, Status status) {
    if (!status.ok()) {
        ESP_LOGW(
            kTag,
            "%s failed: error=%d",
            operation,
            static_cast<int>(status.code()));
    }
}

std::uint32_t next_loop_delay_ms(std::uint64_t now_us) {
    const DeviceState state = lifecycle.snapshot().state;
    const bool sleep_transition_active =
        state == DeviceState::Sleeping &&
        (now_us < last_activity_us ||
         now_us - last_activity_us < Product::Interaction::sleep_transition_ms * 1000ULL);
    std::uint32_t delay_ms = base_runtime_delay_ms(
        state,
        haptic.active(),
        sleep_transition_active,
        RuntimeScheduleConfig{
            Product::Interaction::system_tick_ms,
            Product::Interaction::fault_tick_ms,
            Product::Interaction::sleeping_tick_ms,
        });
    delay_ms = touch.next_poll_delay_ms(now_us / 1000ULL, delay_ms);
    if (state == DeviceState::Sleeping) {
        delay_ms = climate_port.next_poll_delay_ms(now_us, delay_ms);
    }
    return delay_ms;
}

void wait_for_runtime_event(std::uint32_t maximum_delay_ms) {
    TickType_t delay_ticks = pdMS_TO_TICKS(maximum_delay_ms);
    if (delay_ticks == 0) {
        delay_ticks = 1;
    }
    // 每次只消费一个通知计数。若 BLE 在一次业务 Tick 内连续入队多个帧，剩余计数会让
    // 后续循环立即继续处理，不会因为清空通知而把队列中的命令拖到兜底超时。
    (void)ulTaskNotifyTake(pdFALSE, delay_ticks);
}

bool initialize_hardware() {
    // 闭环位置、输出、触摸唤醒、电源和 BLE 是可安全运行的必要能力。
    // 环境传感器允许单项降级：初始化失败后其 poll 会产生 SensorFault 快照，
    // 但设备仍可绑定、上报故障、OTA 和执行不依赖该传感器的交互。
    bool critical_ok = true;
    critical_ok = adc.initialize().ok() && critical_ok;
    const Status i2c_status = i2c.initialize();
    critical_ok = motion.initialize().ok() && critical_ok;
    critical_ok = led_output.initialize().ok() && critical_ok;
    critical_ok = haptic.initialize().ok() && critical_ok;
    critical_ok = touch.initialize().ok() && critical_ok;
    critical_ok = power_port.initialize().ok() && critical_ok;

    const Status acoustic_status = acoustic_port.initialize();
    const Status illumination_status = illumination_port.initialize();
    const Status climate_status = climate_port.initialize();
    const Status battery_status = battery_port.initialize();
    if (!i2c_status.ok() || !acoustic_status.ok() || !illumination_status.ok() ||
        !climate_status.ok() || !battery_status.ok()) {
        ESP_LOGW(kTag, "one or more optional sensors started in degraded mode");
    }

    critical_ok = communication.initialize().ok() && critical_ok;
    return critical_ok;
}

ErrorCode active_fault() {
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
    return lifecycle.snapshot().state == DeviceState::Fault
               ? ErrorCode::InternalFailure
               : ErrorCode::None;
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
            application.growth_motion_active() ? application.growth_motion_behavior()
                                               : behavior_state.behavior,
            ota_state.state,
            static_cast<std::uint32_t>(ota_state.received_bytes),
            Product::Product::firmware_version,
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
        Product::Product::device_name,
        Product::Product::hardware_revision,
        Product::Product::firmware_version,
        boot_ok ? "ready" : "fault");
}

bool position_is_safe_for_deep_sleep() {
    const PositionSnapshot position = motion.position_snapshot();
    // Sleep 不再改变生长高度；Behavior Service 已停止 PWM 并关闭舵机电源，因此深睡
    // 只需确认反馈仍有效且没有未完成的生长/衰减运动。
    return position.feedback == PositionFeedbackState::Valid && !position.moving;
}

}  // namespace

void initialize() {
    system_task_handle = xTaskGetCurrentTaskHandle();
#if !CONFIG_SECURE_SIGNED_ON_UPDATE
    ESP_LOGW(
        kTag,
        "development OTA build: cryptographic image verification is disabled");
#endif
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
            report_tick_status(application.tick(now_us));
            if (application.boot_sensors_ready() ||
                now_us - boot_started_us >= kBootSensorTimeoutUs) {
                finish_boot(application.boot_critical_sensors_ok(), now_us);
            }
            wait_for_runtime_event(Product::Interaction::system_tick_ms);
            continue;
        }

        const DeviceState lifecycle_before_tick = lifecycle.snapshot().state;
        report_tick_status(application.tick(now_us));

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
            report_event_failure(
                "communication connected",
                application.handle_communication_connected());
        }
        if (communication_was_connected && !communication_connected) {
            report_event_failure(
                "communication disconnected",
                application.handle_communication_disconnected());
        }
        communication_was_connected = communication_connected;
        communication_was_secure = communication_secure;

        TouchGesture gesture{};
        if (touch.poll(now_us / 1000ULL, gesture)) {
            last_activity_us = now_us;
            const bool waking_from_sleep =
                lifecycle.snapshot().state == DeviceState::Sleeping;
            if (gesture == TouchGesture::FactoryResetHold) {
                const Status reset_status = ble.forget_bonds();
                if (reset_status.ok()) {
                    ESP_LOGW(kTag, "local recovery cleared all BLE bonds");
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                }
                ESP_LOGE(kTag, "failed to clear BLE bonds");
            } else {
                const Status touch_status = application.handle_touch(gesture, now_us);
                if (touch_status.ok() && waking_from_sleep) {
                    report_event_failure("fast advertising", ble.request_fast_advertising());
                } else if (!touch_status.ok() && touch_status.code() != ErrorCode::Busy) {
                    report_event_failure("touch", touch_status);
                }
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
                Product::Interaction::automatic_sleep_ms * 1000ULL) {
            last_activity_us = now_us;
            report_event_failure("idle timeout", application.handle_idle_timeout());
        }
        if (lifecycle_state.state == DeviceState::Sleeping &&
            lifecycle_state.power_mode == PowerMode::LightSleep &&
            Product::Power::deep_sleep_enabled &&
            Product::Power::deep_sleep_delay_ms != 0 &&
            position_is_safe_for_deep_sleep() &&
            now_us - last_activity_us >= Product::Power::deep_sleep_delay_ms * 1000ULL) {
            const OtaState ota_state = ota.snapshot().state;
            report_event_failure(
                "deep sleep",
                power.request_deep_sleep(PowerConditions{
                    communication.connected(),
                    ota_state == OtaState::Receiving || ota_state == OtaState::Verifying,
                    // V2 当前没有运行时配置写入；加入持久化后必须接入真实 Storage busy。
                    false,
                }));
        }
        wait_for_runtime_event(next_loop_delay_ms(now_us));
    }
}

}  // namespace plant::product::v2
