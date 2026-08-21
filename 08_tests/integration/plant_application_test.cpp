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

    MotionPattern pattern{MotionPattern::ReturnNeutral};
    std::uint32_t execution_id{0};
};

class AppLight final : public ILightPort {
public:
    Status play(LightPattern value, std::uint32_t) override {
        pattern = value;
        return Status::success();
    }
    Status stop() override { return Status::success(); }

    LightPattern pattern{LightPattern::SlowBreathing};
};

class AppHaptic final : public IHapticPort {
public:
    Status play(HapticPattern value, std::uint32_t) override {
        pattern = value;
        return Status::success();
    }
    Status stop() override { return Status::success(); }

    HapticPattern pattern{HapticPattern::Off};
};

class AppPower final : public IPowerPort {
public:
    Status enter_light_sleep() override {
        ++light_sleep_count;
        return Status::success();
    }
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
    Status abort() override { return Status::success(); }

    int begin_count{0};
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
    (void)fixture.app.handle_behavior_event({BehaviorEventType::MotionCompleted, id});
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
    CHECK_APP(fixture.app.cancel_ota().ok());
    complete_motion(fixture);
    CHECK_APP(fixture.lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK_APP(fixture.power_port.light_sleep_count == 1);
    CHECK_APP(fixture.ota_port.begin_count == 0);
}

}  // namespace

int run_plant_application_tests() {
    test_touch_happy_flow();
    test_auto_sleep_then_touch_wakeup();
    test_ota_waits_for_safe_sleep_pose();
    test_invalid_ota_does_not_move_plant();
    test_stop_is_idempotent_and_pending_ota_can_be_cancelled();
    return failures;
}

}  // namespace plant::test
