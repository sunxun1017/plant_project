#include <cstddef>
#include <cstdint>

#include "07_products/plant_v1/app/plant_application.hpp"

namespace plant::test {
namespace {

class AppMotion final : public IMotionPort {
public:
    Status play(MotionPattern value, std::uint32_t id) override {
        pattern = value;
        execution_id = id;
        return Status::success();
    }
    Status stop() override { return Status::success(); }
    Status poll(std::uint64_t, MotionPollResult& result) override {
        result = poll_result;
        poll_result = MotionPollResult{};
        return poll_status;
    }

    MotionPattern pattern{MotionPattern::ReturnNeutral};
    std::uint32_t execution_id{0};
    MotionPollResult poll_result{};
    Status poll_status{};
};

class AppLight final : public ILightPort {
public:
    Status play(LightPattern value, std::uint32_t) override {
        pattern = value;
        return Status::success();
    }
    Status stop() override { return Status::success(); }
    Status tick(std::uint64_t) override { return tick_status; }

    LightPattern pattern{LightPattern::SlowBreathing};
    Status tick_status{};
};

class AppHaptic final : public IHapticPort {
public:
    Status play(HapticPattern value, std::uint32_t) override {
        pattern = value;
        return Status::success();
    }
    Status stop() override { return Status::success(); }
    Status tick(std::uint64_t) override { return tick_status; }

    HapticPattern pattern{HapticPattern::Off};
    Status tick_status{};
};

class AppPower final : public IPowerPort {
public:
    Status enter_light_sleep() override {
        ++light_sleep_count;
        return Status::success();
    }
    Status leave_light_sleep() override { return Status::success(); }
    Status enter_deep_sleep(std::uint64_t) override { return Status::success(); }
    WakeSource wake_source() const override { return WakeSource::Touch; }

    int light_sleep_count{0};
};

class AppOta final : public IOtaPort {
public:
    std::size_t available_image_space() const override { return 4096; }
    Status begin(const OtaImageMetadata&) override {
        ++begin_count;
        return Status::success();
    }
    Status write(std::size_t, const std::uint8_t*, std::size_t) override {
        return Status::success();
    }
    Status verify_and_activate(const OtaImageMetadata&) override { return Status::success(); }
    Status abort() override {
        ++abort_count;
        return Status::success();
    }

    int begin_count{0};
    int abort_count{0};
};

struct AppFixture {
    AppMotion motion;
    AppLight light;
    AppHaptic haptic;
    AppPower power_port;
    AppOta ota_port;
    BehaviorService behavior{motion, light, haptic};
    LifecycleService lifecycle;
    PowerService power{lifecycle, power_port};
    OtaService ota{lifecycle, ota_port, {0x504C414E, 1, 1}};
    PlantApplication app{behavior, lifecycle, power, ota};
};

int failures = 0;

#define CHECK_APP(condition)                                                                 \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            ++failures;                                                                      \
        }                                                                                    \
    } while (false)

void complete_motion(AppFixture& fixture) {
    const auto id = fixture.behavior.snapshot().execution_id;
    fixture.motion.poll_result = MotionPollResult{true, id};
    (void)fixture.app.tick(1000);
}

void test_touch_happy_flow() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());
    CHECK_APP(fixture.app.handle_touch(TouchGesture::SingleTap).ok());
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Interacting);
    CHECK_APP(fixture.motion.pattern == MotionPattern::GentleSway);
    CHECK_APP(fixture.light.pattern == LightPattern::SoftBreathing);
    CHECK_APP(fixture.haptic.pattern == HapticPattern::DoubleSoftPulse);
    complete_motion(fixture);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Idle);
}

void test_auto_sleep_then_touch_wakeup() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());
    CHECK_APP(fixture.app.handle_idle_timeout().ok());
    CHECK_APP(fixture.motion.pattern == MotionPattern::MoveToSleepPose);
    complete_motion(fixture);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK_APP(fixture.lifecycle.snapshot().power_mode == PowerMode::LightSleep);
    CHECK_APP(fixture.power_port.light_sleep_count == 1);

    CHECK_APP(fixture.app.handle_touch(TouchGesture::SingleTap).ok());
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Interacting);
    CHECK_APP(fixture.motion.pattern == MotionPattern::Wake);
}

void test_ota_waits_for_safe_sleep_pose() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());
    const OtaImageMetadata metadata{0x504C414E, 1, 2, 4, true};

    CHECK_APP(fixture.app.begin_ota(metadata).ok());
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Interacting);
    CHECK_APP(fixture.motion.pattern == MotionPattern::MoveToSleepPose);
    CHECK_APP(fixture.ota_port.begin_count == 0);

    complete_motion(fixture);
    CHECK_APP(fixture.ota_port.begin_count == 1);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Updating);
    CHECK_APP(fixture.power_port.light_sleep_count == 0);
}

void test_invalid_ota_does_not_move_plant() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());
    const OtaImageMetadata metadata{0xBAD, 1, 2, 4, true};

    CHECK_APP(fixture.app.begin_ota(metadata).code() == ErrorCode::InvalidArgument);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Idle);
    CHECK_APP(fixture.behavior.snapshot().state == BehaviorRunState::Idle);
}

void test_stop_is_idempotent_and_pending_ota_can_be_cancelled() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());
    CHECK_APP(fixture.app.stop_behavior().ok());

    const OtaImageMetadata metadata{0x504C414E, 1, 2, 4, true};
    CHECK_APP(fixture.app.begin_ota(metadata).ok());
    CHECK_APP(fixture.app.handle_communication_disconnected().ok());
    complete_motion(fixture);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK_APP(fixture.power_port.light_sleep_count == 1);
    CHECK_APP(fixture.ota_port.begin_count == 0);
}

void test_ble_command_dispatch_uses_same_application_rules() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());
    Command command{};
    command.type = CommandType::SetBehavior;
    command.behavior = Behavior::Attention;

    CHECK_APP(fixture.app.handle_command(command).ok());
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Interacting);
    CHECK_APP(fixture.motion.pattern == MotionPattern::LookUp);

    command.type = CommandType::GetState;
    CHECK_APP(fixture.app.handle_command(command).ok());
}

void test_wake_and_sleep_commands_are_idempotent_in_terminal_states() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());

    CHECK_APP(fixture.app.request_behavior(Behavior::WakeUp).ok());
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Idle);
    CHECK_APP(fixture.motion.execution_id == 0);

    CHECK_APP(fixture.app.handle_idle_timeout().ok());
    complete_motion(fixture);
    const auto sleep_execution_id = fixture.motion.execution_id;
    CHECK_APP(fixture.app.request_behavior(Behavior::Sleep).ok());
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK_APP(fixture.motion.execution_id == sleep_execution_id);
}

void test_disconnect_cancels_active_ota_session() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());
    const OtaImageMetadata metadata{0x504C414E, 1, 2, 4, true};

    CHECK_APP(fixture.app.begin_ota(metadata).ok());
    complete_motion(fixture);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Updating);
    CHECK_APP(fixture.app.handle_communication_disconnected().ok());
    CHECK_APP(fixture.ota_port.abort_count == 1);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Idle);
}

void test_connection_plays_attention_and_wakes_sleeping_plant() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());

    CHECK_APP(fixture.app.handle_communication_connected().ok());
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Interacting);
    CHECK_APP(fixture.motion.pattern == MotionPattern::LookUp);
    const auto attention_execution_id = fixture.motion.execution_id;
    CHECK_APP(fixture.app.handle_communication_connected().ok());
    CHECK_APP(fixture.motion.execution_id == attention_execution_id);
    complete_motion(fixture);

    CHECK_APP(fixture.app.handle_idle_timeout().ok());
    complete_motion(fixture);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK_APP(fixture.app.handle_communication_connected().ok());
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Interacting);
    CHECK_APP(fixture.motion.pattern == MotionPattern::Wake);
}

void test_runtime_actuator_failure_updates_lifecycle_fault() {
    AppFixture fixture;
    CHECK_APP(fixture.app.finish_boot(true).ok());
    CHECK_APP(fixture.app.request_behavior(Behavior::Happy).ok());
    fixture.motion.poll_status = Status::failure(ErrorCode::MotionFailure);

    CHECK_APP(fixture.app.tick(1000).code() == ErrorCode::MotionFailure);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Fault);
    CHECK_APP(fixture.behavior.snapshot().state == BehaviorRunState::Fault);
}

}  // namespace

int run_plant_application_tests() {
    test_touch_happy_flow();
    test_auto_sleep_then_touch_wakeup();
    test_ota_waits_for_safe_sleep_pose();
    test_invalid_ota_does_not_move_plant();
    test_stop_is_idempotent_and_pending_ota_can_be_cancelled();
    test_ble_command_dispatch_uses_same_application_rules();
    test_wake_and_sleep_commands_are_idempotent_in_terminal_states();
    test_disconnect_cancels_active_ota_session();
    test_connection_plays_attention_and_wakes_sleeping_plant();
    test_runtime_actuator_failure_updates_lifecycle_fault();
    return failures;
}

}  // namespace plant::test
