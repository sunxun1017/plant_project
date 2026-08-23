#include <cstddef>
#include <cstdint>

#include "05_adapters/common/bounded_retry_backoff.hpp"
#include "10_config/plant_v2/plant_v2_product_config.hpp"
#include "07_products/plant_v2/app/plant_v2_application.hpp"
#include "07_products/plant_v2/runtime_schedule.hpp"

namespace plant::test {
namespace {

int failures = 0;

#define CHECK_V2_APP(condition) \
    do {                        \
        if (!(condition)) {     \
            ++failures;         \
        }                       \
    } while (false)

class V2Motion final : public IPositionMotionPort {
public:
    Status move_to(MotionPattern value, std::uint16_t target, std::uint32_t id) override {
        pattern = value;
        snapshot.target_position = target;
        snapshot.moving = true;
        move_target = target;
        move_execution_id = id;
        return move_status;
    }
    Status stop() override {
        snapshot.moving = false;
        return Status::success();
    }
    Status poll(std::uint64_t, MotionPollResult& result) override {
        if (!poll_status.ok()) {
            return poll_status;
        }
        result = poll_result;
        poll_result = MotionPollResult{};
        if (result.completed) {
            snapshot.moving = false;
        }
        return Status::success();
    }
    PositionSnapshot position_snapshot() const noexcept override { return snapshot; }

    PositionSnapshot snapshot{400, 400, PositionFeedbackState::Valid};
    MotionPattern pattern{MotionPattern::Grow};
    std::uint16_t move_target{0};
    std::uint32_t move_execution_id{0};
    MotionPollResult poll_result{};
    Status poll_status{};
    Status move_status{};
};

class V2LightOutput final : public ILightPort {
public:
    Status play(LightPattern value, std::uint32_t) override {
        pattern = value;
        return Status::success();
    }
    Status stop() override { return Status::success(); }
    Status tick(std::uint64_t) override { return Status::success(); }
    Status set_intensity(std::uint16_t value) override {
        intensity = value;
        return Status::success();
    }

    LightPattern pattern{LightPattern::FadeOut};
    std::uint16_t intensity{0};
};

class V2Haptic final : public IHapticPort {
public:
    Status play(HapticPattern, std::uint32_t) override { return Status::success(); }
    Status stop() override {
        output_active = false;
        return Status::success();
    }
    Status tick(std::uint64_t) override { return Status::success(); }
    bool active() const noexcept override { return output_active; }

    bool output_active{false};
};

class V2Power final : public IPowerPort {
public:
    Status enter_light_sleep() override { return Status::success(); }
    Status leave_light_sleep() override { return Status::success(); }
    Status enter_deep_sleep(std::uint64_t) override { return Status::success(); }
    WakeSource wake_source() const override { return WakeSource::Touch; }
};

class V2Ota final : public IOtaPort {
public:
    Status finalize_boot(bool) override { return Status::success(); }
    std::size_t available_image_space() const override { return 4096; }
    Status begin(const OtaImageMetadata&) override { return Status::success(); }
    Status write(std::size_t, const std::uint8_t*, std::size_t) override {
        return Status::success();
    }
    Status verify_and_activate(const OtaImageMetadata&) override { return Status::success(); }
    Status abort() override { return Status::success(); }
};

template <typename Sample>
struct QueuedSample {
    Sample sample{};
    bool available{false};
    Status status{};
};

class V2AcousticPort final : public IAcousticSensorPort {
public:
    Status initialize() override { return Status::success(); }
    Status set_enabled(bool value) override {
        enabled = value;
        return Status::success();
    }
    Status poll(std::uint64_t, AcousticSample& sample, bool& available) override {
        sample = queued.sample;
        available = queued.available;
        queued.available = false;
        return queued.status;
    }
    QueuedSample<AcousticSample> queued{};
    bool enabled{true};
};

class V2IlluminationPort final : public IIlluminationSensorPort {
public:
    Status initialize() override { return Status::success(); }
    Status poll(std::uint64_t, IlluminationSample& sample, bool& available) override {
        sample = queued.sample;
        available = queued.available;
        queued.available = false;
        return queued.status;
    }
    QueuedSample<IlluminationSample> queued{};
};

class V2ClimatePort final : public IClimateSensorPort {
public:
    Status initialize() override { return Status::success(); }
    Status set_enabled(bool value) override {
        enabled = value;
        return Status::success();
    }
    Status poll(std::uint64_t, ClimateSample& sample, bool& available) override {
        sample = queued.sample;
        available = queued.available;
        queued.available = false;
        return queued.status;
    }
    QueuedSample<ClimateSample> queued{};
    bool enabled{true};
};

class V2BatteryPort final : public IBatterySensorPort {
public:
    Status initialize() override { return Status::success(); }
    Status poll(std::uint64_t, BatterySample& sample, bool& available) override {
        sample = queued.sample;
        available = queued.available;
        queued.available = false;
        return queued.status;
    }
    QueuedSample<BatterySample> queued{};
};

struct V2Fixture {
    V2Motion motion;
    V2LightOutput light_output;
    LightArbitrationService light{light_output};
    V2Haptic haptic;
    V2Power power_port;
    V2Ota ota_port;
    V2AcousticPort acoustic_port;
    V2IlluminationPort illumination_port;
    V2ClimatePort climate_port;
    V2BatteryPort battery_port;
    BehaviorService behavior{
        motion, light, haptic, BehaviorExecutionConfig{5000000}};
    LifecycleService lifecycle;
    PowerService power{lifecycle, power_port};
    OtaService ota{lifecycle, ota_port, {0x504C414E, 2, 0x00020000}};
    PlantApplication base{behavior, lifecycle, power, ota};
    AcousticService acoustic;
    IlluminationService illumination;
    ClimateService climate;
    BatteryService battery{200, 80};
    GrowthService growth{GrowthConfig{
        config::v2::ProductConfig::Growth::step,
        900,
        15,
        config::v2::ProductConfig::Growth::pending_expiry_ms * 1000ULL,
        {0, 0, 0, 0, 0},
        100,
        config::v2::ProductConfig::Growth::decay_step,
        config::v2::ProductConfig::Growth::inactivity_before_decay_ms * 1000ULL,
        config::v2::ProductConfig::Growth::decay_interval_ms * 1000ULL,
        config::v2::ProductConfig::Growth::maximum_pending_credits,
    }};
    PlantV2Application app{
        base,
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
        100000,
    };
};

void queue_valid_boot_samples(V2Fixture& fixture) {
    fixture.acoustic_port.queued = {AcousticSample{50, true}, true, Status::success()};
    fixture.illumination_port.queued = {
        IlluminationSample{350, true}, true, Status::success()};
    fixture.climate_port.queued = {
        ClimateSample{2300, 500, true}, true, Status::success()};
    fixture.battery_port.queued = {
        BatterySample{3900, 750, true}, true, Status::success()};
}

void test_boot_sensor_gate_and_telemetry_snapshots() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    CHECK_V2_APP(!fixture.app.boot_sensors_ready());
    queue_valid_boot_samples(fixture);
    CHECK_V2_APP(fixture.app.tick(1000).ok());
    CHECK_V2_APP(fixture.app.boot_sensors_ready());
    CHECK_V2_APP(fixture.app.boot_critical_sensors_ok());
    CHECK_V2_APP(fixture.app.climate_snapshot().temperature_centi_c == 2300);
    CHECK_V2_APP(fixture.app.battery_snapshot().level_per_mille == 750);
}

void test_optional_sensor_faults_do_not_fail_critical_boot_gate() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    fixture.acoustic_port.queued.status = Status::failure(ErrorCode::SensorFailure);
    fixture.illumination_port.queued.status = Status::failure(ErrorCode::SensorFailure);
    fixture.climate_port.queued.status = Status::failure(ErrorCode::SensorFailure);
    fixture.battery_port.queued.status = Status::failure(ErrorCode::SensorFailure);

    CHECK_V2_APP(fixture.app.tick(1000).ok());
    CHECK_V2_APP(fixture.app.boot_sensors_ready());
    CHECK_V2_APP(fixture.app.boot_critical_sensors_ok());
    CHECK_V2_APP(fixture.app.acoustic_snapshot().state == AcousticState::SensorFault);
    CHECK_V2_APP(
        fixture.app.illumination_snapshot().state == IlluminationState::SensorFault);
    CHECK_V2_APP(fixture.app.climate_snapshot().state == ClimateState::SensorFault);
    CHECK_V2_APP(fixture.app.battery_snapshot().state == BatteryState::SensorFault);

    fixture.motion.snapshot.feedback = PositionFeedbackState::OpenCircuit;
    CHECK_V2_APP(!fixture.app.boot_critical_sensors_ok());
}

void test_light_sleep_keeps_environment_sampling_and_queues_growth() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    CHECK_V2_APP(fixture.lifecycle.begin_behavior(Behavior::Sleep).ok());
    CHECK_V2_APP(
        fixture.lifecycle.apply_behavior_outcome(BehaviorOutcome::CompletedSleeping).ok());
    CHECK_V2_APP(fixture.lifecycle.enter_light_sleep().ok());

    fixture.acoustic_port.queued = {AcousticSample{900, true}, true, Status::success()};
    fixture.climate_port.queued = {
        ClimateSample{2300, 500, true}, true, Status::success()};
    CHECK_V2_APP(fixture.app.tick(1000).ok());
    CHECK_V2_APP(!fixture.acoustic_port.enabled);
    CHECK_V2_APP(fixture.acoustic_port.queued.available);
    CHECK_V2_APP(fixture.climate_port.enabled);
    CHECK_V2_APP(fixture.app.climate_snapshot().state == ClimateState::Suitable);

    fixture.climate_port.queued = {
        ClimateSample{2300, 500, true}, true, Status::success()};
    CHECK_V2_APP(fixture.app.tick(600001001).ok());
    CHECK_V2_APP(fixture.app.growth_snapshot().pending);
    CHECK_V2_APP(!fixture.app.growth_motion_active());

    // 产品配置不让睡眠期间的有效互动过期；超过旧测试使用的 30 秒仍应保留。
    CHECK_V2_APP(fixture.app.tick(640001001).ok());
    CHECK_V2_APP(fixture.app.growth_snapshot().pending);
    CHECK_V2_APP(fixture.lifecycle.wake(false).ok());
    CHECK_V2_APP(fixture.app.tick(640002001).ok());
    CHECK_V2_APP(fixture.acoustic_port.enabled);
    CHECK_V2_APP(fixture.app.growth_motion_active());
    CHECK_V2_APP(fixture.motion.move_target == 450);
    CHECK_V2_APP(fixture.motion.pattern == MotionPattern::Grow);
}

void test_acoustic_mask_includes_actuator_recovery_window() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    fixture.motion.snapshot.moving = true;
    fixture.acoustic_port.queued = {
        AcousticSample{900, true}, true, Status::success()};
    CHECK_V2_APP(fixture.app.tick(1000).ok());
    CHECK_V2_APP(fixture.app.acoustic_snapshot().interference_masked);

    fixture.motion.snapshot.moving = false;
    fixture.acoustic_port.queued = {
        AcousticSample{900, true}, true, Status::success()};
    CHECK_V2_APP(fixture.app.tick(50000).ok());
    CHECK_V2_APP(fixture.app.acoustic_snapshot().interference_masked);

    fixture.acoustic_port.queued = {
        AcousticSample{900, true}, true, Status::success()};
    CHECK_V2_APP(fixture.app.tick(102000).ok());
    CHECK_V2_APP(!fixture.app.acoustic_snapshot().interference_masked);
}

void test_acoustic_mask_includes_haptic_output_and_recovery_window() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    fixture.haptic.output_active = true;
    fixture.acoustic_port.queued = {
        AcousticSample{900, true}, true, Status::success()};
    CHECK_V2_APP(fixture.app.tick(1000).ok());
    CHECK_V2_APP(fixture.app.acoustic_snapshot().interference_masked);

    fixture.haptic.output_active = false;
    fixture.acoustic_port.queued = {
        AcousticSample{900, true}, true, Status::success()};
    CHECK_V2_APP(fixture.app.tick(50000).ok());
    CHECK_V2_APP(fixture.app.acoustic_snapshot().interference_masked);

    fixture.acoustic_port.queued = {
        AcousticSample{900, true}, true, Status::success()};
    CHECK_V2_APP(fixture.app.tick(102000).ok());
    CHECK_V2_APP(!fixture.app.acoustic_snapshot().interference_masked);
}

void test_touch_credit_uses_measured_position_and_closes_motion_loop() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    CHECK_V2_APP(fixture.app.handle_touch(TouchGesture::SingleTap, 1000).ok());
    const std::uint32_t behavior_id = fixture.behavior.snapshot().execution_id;
    fixture.motion.poll_result = MotionPollResult{true, behavior_id};

    CHECK_V2_APP(fixture.app.tick(2000).ok());
    CHECK_V2_APP(fixture.app.growth_motion_active());
    CHECK_V2_APP(fixture.motion.move_target == 450);
    CHECK_V2_APP(fixture.lifecycle.snapshot().state == DeviceState::Interacting);

    fixture.motion.snapshot.actual_position = 450;
    fixture.motion.snapshot.feedback = PositionFeedbackState::Valid;
    fixture.motion.snapshot.target_reached = true;
    fixture.motion.poll_result = MotionPollResult{true, fixture.motion.move_execution_id};
    CHECK_V2_APP(fixture.app.tick(3000).ok());
    CHECK_V2_APP(!fixture.app.growth_motion_active());
    CHECK_V2_APP(fixture.lifecycle.snapshot().state == DeviceState::Idle);
}

void test_touch_behavior_does_not_move_growth_servo() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());

    CHECK_V2_APP(fixture.app.handle_touch(TouchGesture::SingleTap, 1000).ok());
    CHECK_V2_APP(!fixture.motion.snapshot.moving);
}

void test_inactivity_decay_wakes_light_sleep_moves_down_and_returns_to_sleep() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    CHECK_V2_APP(fixture.app.tick(0).ok());
    CHECK_V2_APP(fixture.lifecycle.begin_behavior(Behavior::Sleep).ok());
    CHECK_V2_APP(
        fixture.lifecycle.apply_behavior_outcome(BehaviorOutcome::CompletedSleeping).ok());
    CHECK_V2_APP(fixture.lifecycle.enter_light_sleep().ok());

    constexpr std::uint64_t inactivity_us =
        6ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    CHECK_V2_APP(fixture.app.tick(inactivity_us).ok());
    CHECK_V2_APP(fixture.app.growth_motion_active());
    CHECK_V2_APP(fixture.app.growth_motion_behavior() == Behavior::Retract);
    CHECK_V2_APP(fixture.motion.move_target == 390);
    CHECK_V2_APP(fixture.motion.pattern == MotionPattern::Retract);
    CHECK_V2_APP(fixture.lifecycle.snapshot().state == DeviceState::Interacting);

    fixture.motion.snapshot.actual_position = 390;
    fixture.motion.snapshot.feedback = PositionFeedbackState::Valid;
    fixture.motion.poll_result =
        MotionPollResult{true, fixture.motion.move_execution_id};
    CHECK_V2_APP(fixture.app.tick(inactivity_us + 1000).ok());
    CHECK_V2_APP(!fixture.app.growth_motion_active());
    CHECK_V2_APP(fixture.app.tick(inactivity_us + 2000).ok());
    CHECK_V2_APP(fixture.lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK_V2_APP(fixture.lifecycle.snapshot().power_mode == PowerMode::LightSleep);
}

void test_idle_position_failure_enters_fault() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    fixture.motion.poll_status = Status::failure(ErrorCode::MotionFailure);
    CHECK_V2_APP(fixture.app.tick(1000).code() == ErrorCode::MotionFailure);
    CHECK_V2_APP(fixture.lifecycle.snapshot().state == DeviceState::Fault);
}

void test_unconfirmed_idle_position_glitch_pauses_tick_without_fault() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(true).ok());
    fixture.motion.poll_status = Status::failure(ErrorCode::Busy);
    fixture.acoustic_port.queued = {
        AcousticSample{900, true}, true, Status::success()};

    CHECK_V2_APP(fixture.app.tick(1000).ok());
    CHECK_V2_APP(fixture.lifecycle.snapshot().state == DeviceState::Idle);
    CHECK_V2_APP(fixture.acoustic_port.queued.available);
    CHECK_V2_APP(!fixture.app.growth_motion_active());
}

void test_fault_recovery_ota_isolated_from_failed_motion_polling() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(false).ok());
    Command command{};
    command.type = CommandType::BeginOta;
    command.ota_metadata =
        OtaImageMetadata{0x504C414E, 2, 0x00020001, 4, true};

    CHECK_V2_APP(fixture.app.handle_command(command).ok());
    CHECK_V2_APP(fixture.lifecycle.snapshot().state == DeviceState::Updating);
    fixture.motion.poll_status = Status::failure(ErrorCode::MotionFailure);
    CHECK_V2_APP(fixture.app.tick(1000).ok());
    CHECK_V2_APP(fixture.lifecycle.snapshot().state == DeviceState::Updating);
    CHECK_V2_APP(!fixture.acoustic_port.enabled);
}

void test_fault_disables_acoustic_sampling() {
    V2Fixture fixture;
    CHECK_V2_APP(fixture.base.finish_boot(false).ok());
    CHECK_V2_APP(fixture.app.tick(1000).ok());
    CHECK_V2_APP(!fixture.acoustic_port.enabled);
}

void test_runtime_schedule_slows_only_safe_states() {
    constexpr product::v2::RuntimeScheduleConfig config{20, 500, 1000};
    CHECK_V2_APP(
        product::v2::base_runtime_delay_ms(DeviceState::Idle, false, false, config) == 20);
    CHECK_V2_APP(
        product::v2::base_runtime_delay_ms(DeviceState::Interacting, false, false, config) ==
        20);
    CHECK_V2_APP(
        product::v2::base_runtime_delay_ms(DeviceState::Fault, false, false, config) == 500);
    CHECK_V2_APP(
        product::v2::base_runtime_delay_ms(DeviceState::Sleeping, false, false, config) ==
        1000);
    CHECK_V2_APP(
        product::v2::base_runtime_delay_ms(DeviceState::Sleeping, true, false, config) == 20);
    CHECK_V2_APP(
        product::v2::base_runtime_delay_ms(DeviceState::Sleeping, false, true, config) == 20);
}

void test_bounded_retry_switches_to_fault_backoff() {
    const BoundedRetryDecision first = bounded_retry_backoff(0, 3, 100, 30000);
    CHECK_V2_APP(first.next_failure_count == 1);
    CHECK_V2_APP(first.delay_ms == 100);
    CHECK_V2_APP(!first.report_failure);

    const BoundedRetryDecision second =
        bounded_retry_backoff(first.next_failure_count, 3, 100, 30000);
    CHECK_V2_APP(second.next_failure_count == 2);
    CHECK_V2_APP(!second.report_failure);

    const BoundedRetryDecision exhausted =
        bounded_retry_backoff(second.next_failure_count, 3, 100, 30000);
    CHECK_V2_APP(exhausted.next_failure_count == 0);
    CHECK_V2_APP(exhausted.delay_ms == 30000);
    CHECK_V2_APP(exhausted.report_failure);
}

}  // namespace

int run_plant_v2_application_tests() {
    test_boot_sensor_gate_and_telemetry_snapshots();
    test_optional_sensor_faults_do_not_fail_critical_boot_gate();
    test_light_sleep_keeps_environment_sampling_and_queues_growth();
    test_acoustic_mask_includes_actuator_recovery_window();
    test_acoustic_mask_includes_haptic_output_and_recovery_window();
    test_touch_credit_uses_measured_position_and_closes_motion_loop();
    test_touch_behavior_does_not_move_growth_servo();
    test_inactivity_decay_wakes_light_sleep_moves_down_and_returns_to_sleep();
    test_unconfirmed_idle_position_glitch_pauses_tick_without_fault();
    test_idle_position_failure_enters_fault();
    test_fault_recovery_ota_isolated_from_failed_motion_polling();
    test_fault_disables_acoustic_sampling();
    test_runtime_schedule_slows_only_safe_states();
    test_bounded_retry_switches_to_fault_backoff();
    return failures;
}

}  // namespace plant::test
