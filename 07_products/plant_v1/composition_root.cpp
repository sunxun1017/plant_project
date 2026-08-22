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

EspServoAdapter motion; // 舵机运动适配器。
EspLedAdapter light; // RGB 灯光适配器。
EspVibrationAdapter haptic; // 振动反馈适配器。
EspTouchAdapter touch; // 触摸输入适配器。
EspBleAdapter ble; // BLE 传输适配器。
EspPowerAdapter power_port; // ESP-IDF 功耗控制适配器。
EspOtaAdapter ota_port; // ESP-IDF OTA Flash 适配器。

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
    // 累积每个初始化结果；调用放在 && 左侧，因此前一项失败后仍会尝试后续设备。
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
    const bool hardware_ok = initialize_hardware(); // 使用 composition root 选定的全部硬件适配器执行启动自检。
    // 硬件失败时报告本次启动失败；只有 PENDING_VERIFY 的 OTA 镜像会实际回滚。
    // 硬件成功时暂不确认镜像，留到稳定运行十秒后处理。
    const Status ota_boot_status =
        hardware_ok ? Status::success() : ota_port.finalize_boot(false);
    // ota_boot_status 表示失败报告操作是否成功，不是一次独立的 OTA 健康检查。
    const bool platform_boot_ok = hardware_ok && ota_boot_status.ok();
    const Status application_boot_status = application.finish_boot(platform_boot_ok);
    const bool boot_ok = platform_boot_ok && application_boot_status.ok();
    if (!application_boot_status.ok()) {
        ESP_LOGE(kTag, "application boot state transition failed");
        // 生命周期无法完成启动时，也把待验证 OTA 镜像报告为启动失败。
        const Status rollback_status = ota_port.finalize_boot(false);
        if (!rollback_status.ok()) {
            ESP_LOGE(kTag, "failed to report application boot failure to OTA rollback");
        }
    }
    last_activity_us = static_cast<std::uint64_t>(esp_timer_get_time());
    boot_confirmation_due_us = last_activity_us + kBootConfirmationDelayUs; // 稳定运行十秒后确认 OTA 镜像。
    boot_confirmation_pending = boot_ok;
    communication_was_connected = communication.connected(); // 初始化 BLE 连接边沿检测的基准状态。
    ESP_LOGI(
        kTag,
        "boot product=%s hw=%" PRIu32 " firmware=0x%08" PRIx32 " status=%s",
        Config::Product::device_name,
        Config::Product::hardware_revision,
        Config::Product::firmware_version,
        boot_ok ? "ready" : "fault");
}

/*
 * System Task 每轮依次推进输出状态机、处理 OTA 启动确认、检测 BLE 边沿、
 * 轮询触摸与命令、处理行为完成和低功耗转换，最后让出 CPU。
 */
[[noreturn]] void run() {
    while (true) { // FreeRTOS 主任务中的非阻塞系统循环。
        const std::uint64_t now_us = static_cast<std::uint64_t>(esp_timer_get_time()); // 本轮统一使用的微秒时间基准。
        // 保存 tick 前的状态，用于检测本轮是否完成了 Interacting -> Idle/Sleeping 转换。
        const DeviceState lifecycle_state_before_tick = lifecycle.snapshot().state;
        (void)application.tick(now_us);

        // 所有成功启动都会进入这里；非 PENDING_VERIFY 镜像由 Adapter 识别后按成功空操作返回。
        if (boot_confirmation_pending && now_us >= boot_confirmation_due_us) {
            // TODO: 当前“自检”只判断生命周期是否为 Fault；若产品需要更强保证，应加入
            // 外设健康、关键服务和持久化状态等明确诊断结果。
            const bool self_test_ok =
                lifecycle.snapshot().state != DeviceState::Fault;
            const Status confirmation_status = ota_port.finalize_boot(self_test_ok);
            // TODO: 确认接口失败后当前实现不会重试，因为 pending 在错误处理前已清除。
            boot_confirmation_pending = false;
            if (!confirmation_status.ok()) {
                // OTA 确认接口失败时发送故障事件，由应用统一进入故障处理。
                (void)application.handle_behavior_event(
                    BehaviorEvent{BehaviorEventType::FaultRaised, 0});
            }
        }

        const bool communication_connected = communication.connected(); // 查询本轮 BLE 连接状态。
        if (!communication_was_connected && communication_connected) { // 检测 false -> true 连接边沿。
            last_activity_us = now_us; // 新连接计为一次用户活动。
            (void)application.handle_communication_connected(); // 根据生命周期触发 WakeUp 或 Attention。
        }
        if (communication_was_connected && !communication_connected) { // 检测 true -> false 断开边沿。
            // 断开时仅在 OTA 已排队、接收或验证过程中取消 OTA。
            (void)application.handle_communication_disconnected();
        }
        communication_was_connected = communication_connected;

        TouchGesture gesture{};
        if (touch.poll(now_us / 1000ULL, gesture)) {
            last_activity_us = now_us;
            (void)application.handle_touch(gesture); // 将稳定手势转换为语义行为。
        }

        Command command{};
        bool command_available = false;
        const Status receive_status = communication.poll(command, command_available); // 非阻塞接收并解码 BLE 命令。
        if (command_available) {
            if (!receive_status.ok()) {
                if (receive_status.code() != ErrorCode::ProtocolFailure) {
                    respond_with_current_state(command, receive_status);
                }
            } else {
                last_activity_us = now_us;
                const Status execution_status = application.handle_command(command); // 执行语义命令。
                respond_with_current_state(command, execution_status); // 返回执行结果和最新状态。
                if (execution_status.ok() && command.type == CommandType::FinishOta) {
                    // 给 BLE 响应留出发送窗口，再重启到刚激活的 OTA 分区。
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                }
            }
        }

        const DeviceState state_after_tick = lifecycle.snapshot().state;
        if (state_after_tick == DeviceState::Idle || // 行为结束后可能回到 Idle 或进入 Sleeping。
            state_after_tick == DeviceState::Sleeping) {
            const BehaviorRunState behavior_state = behavior.snapshot().state;
            if (behavior_state == BehaviorRunState::Idle && // 行为状态机已完成本次执行。
                lifecycle_state_before_tick == DeviceState::Interacting) {
                // 从行为完成时重新计算空闲时间，而不是从行为开始时计算。
                last_activity_us = static_cast<std::uint64_t>(esp_timer_get_time());
            }
        }

        const LifecycleSnapshot lifecycle_state = lifecycle.snapshot();
        if (lifecycle_state.state == DeviceState::Idle &&
            now_us - last_activity_us >=
                static_cast<std::uint64_t>(Config::Interaction::automatic_sleep_ms) * 1000ULL) {
            last_activity_us = now_us;
            (void)application.handle_idle_timeout(); // Idle 达到自动休眠阈值后启动 Sleep 行为。
        }
        // Deep Sleep 前，Sleep 行为已经把舵机移动到 BSP 定义的安全休眠位置。
        // TODO(product): Grow 进入范围后只持久化最后一次经过安全限制的绝对舵机目标命令，
        // 不保存轨迹、行为历史或原始 PWM；相对命令不能直接重放，避免重启后重复位移。
        if (lifecycle_state.state == DeviceState::Sleeping &&
            lifecycle_state.power_mode == PowerMode::LightSleep &&
            Config::Power::deep_sleep_enabled && Config::Power::deep_sleep_delay_ms != 0 &&
            now_us - last_activity_us >=
                static_cast<std::uint64_t>(Config::Power::deep_sleep_delay_ms) * 1000ULL) { // 距最后活动达到深睡眠阈值。
            const OtaState ota_state = ota.snapshot().state;
            (void)power.request_deep_sleep(PowerConditions{
                communication.connected(),
                ota_state == OtaState::Receiving || ota_state == OtaState::Verifying,
                false, // TODO: 尚未接入真实 Flash 写入状态，当前固定假设没有写入活动。
            });
        }
        vTaskDelay(pdMS_TO_TICKS(Config::Interaction::system_tick_ms));
    }
}

}  // namespace plant::product::v1
