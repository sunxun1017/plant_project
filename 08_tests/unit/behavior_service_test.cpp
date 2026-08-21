#include <cstdlib>
#include <iostream>

#include "03_services/behavior/behavior_policy.hpp"
#include "03_services/behavior/behavior_service.hpp"
#include "03_services/lifecycle/lifecycle_service.hpp"

namespace {

using namespace plant;

class FakeMotion final : public IMotionPort {
public:
    Status play(MotionPattern pattern, std::uint32_t execution_id) override {
        last_pattern = pattern;
        last_execution_id = execution_id;
        ++play_count;
        return next_status;
    }

    Status stop() override {
        ++stop_count;
        return Status::success();
    }

    MotionPattern last_pattern{MotionPattern::ReturnNeutral};
    std::uint32_t last_execution_id{0};
    int play_count{0};
    int stop_count{0};
    Status next_status{};
};

class FakeLight final : public ILightPort {
public:
    Status play(LightPattern pattern, std::uint32_t execution_id) override {
        last_pattern = pattern;
        last_execution_id = execution_id;
        ++play_count;
        return next_status;
    }

    Status stop() override {
        ++stop_count;
        return Status::success();
    }

    LightPattern last_pattern{LightPattern::SlowBreathing};
    std::uint32_t last_execution_id{0};
    int play_count{0};
    int stop_count{0};
    Status next_status{};
};

class FakeHaptic final : public IHapticPort {
public:
    Status play(HapticPattern pattern, std::uint32_t execution_id) override {
        last_pattern = pattern;
        last_execution_id = execution_id;
        ++play_count;
        return next_status;
    }

    Status stop() override {
        ++stop_count;
        return Status::success();
    }

    HapticPattern last_pattern{HapticPattern::Off};
    std::uint32_t last_execution_id{0};
    int play_count{0};
    int stop_count{0};
    Status next_status{};
};

int failures = 0;

#define CHECK(condition)                                                                     \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            std::cerr << __FILE__ << ':' << __LINE__ << " CHECK failed: " #condition << '\n'; \
            ++failures;                                                                      \
        }                                                                                    \
    } while (false)

void test_happy_maps_to_three_outputs() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Happy).ok());
    CHECK(service.snapshot().state == BehaviorRunState::Running);
    CHECK(motion.last_pattern == MotionPattern::GentleSway);
    CHECK(light.last_pattern == LightPattern::SoftBreathing);
    CHECK(haptic.last_pattern == HapticPattern::DoubleSoftPulse);
    CHECK(motion.last_execution_id == light.last_execution_id);
    CHECK(light.last_execution_id == haptic.last_execution_id);
}

void test_duplicate_behavior_is_idempotent() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Happy).ok());
    const auto first_id = service.snapshot().execution_id;
    CHECK(service.start(Behavior::Happy).ok());
    CHECK(service.snapshot().execution_id == first_id);
    CHECK(motion.play_count == 1);
}

void test_wakeup_rejects_normal_interruption_but_accepts_sleep_request() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::WakeUp).ok());
    CHECK(service.start(Behavior::Happy).code() == ErrorCode::Busy);
    CHECK(service.start(Behavior::Sleep, InterruptionReason::WakeSleep).ok());
    CHECK(service.snapshot().behavior == Behavior::Sleep);
}

void test_stale_completion_is_ignored() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Happy).ok());
    const auto first_id = service.snapshot().execution_id;
    CHECK(service.start(Behavior::Calm, InterruptionReason::Touch).ok());
    CHECK(service.handle_event({BehaviorEventType::MotionCompleted, first_id}).ok());
    CHECK(service.snapshot().state == BehaviorRunState::Running);
}

void test_sleep_completion_requests_sleeping_lifecycle() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Sleep).ok());
    const auto id = service.snapshot().execution_id;
    CHECK(service.handle_event({BehaviorEventType::MotionCompleted, id}).ok());
    CHECK(service.take_outcome() == BehaviorOutcome::CompletedSleeping);
}

void test_output_failure_enters_fault_and_safe_patterns() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    light.next_status = Status::failure(ErrorCode::LightingFailure);
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Happy).code() == ErrorCode::LightingFailure);
    CHECK(service.snapshot().state == BehaviorRunState::Fault);
    CHECK(service.snapshot().behavior == Behavior::Error);
    CHECK(motion.stop_count == 1);
    CHECK(motion.last_pattern == MotionPattern::StopAndHoldSafe);
    CHECK(haptic.last_pattern == HapticPattern::Warning);
    CHECK(service.take_outcome() == BehaviorOutcome::Faulted);
}

void test_global_fault_is_accepted_while_idle() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.handle_event({BehaviorEventType::FaultRaised, 0}).code() ==
          ErrorCode::InternalFailure);
    CHECK(service.snapshot().state == BehaviorRunState::Fault);
    CHECK(motion.last_pattern == MotionPattern::StopAndHoldSafe);
    CHECK(light.last_pattern == LightPattern::ErrorBlink);
}

void test_lifecycle_sleep_and_power_modes() {
    LifecycleService lifecycle;
    CHECK(lifecycle.snapshot().state == DeviceState::Booting);
    CHECK(lifecycle.finish_boot(true).ok());
    CHECK(lifecycle.begin_behavior(Behavior::Sleep).ok());
    CHECK(lifecycle.apply_behavior_outcome(BehaviorOutcome::CompletedSleeping).ok());
    CHECK(lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK(lifecycle.enter_light_sleep().ok());
    CHECK(lifecycle.snapshot().power_mode == PowerMode::LightSleep);
    CHECK(lifecycle.enter_deep_sleep().ok());
    CHECK(lifecycle.snapshot().power_mode == PowerMode::DeepSleep);
    CHECK(lifecycle.wake(true).ok());
    CHECK(lifecycle.snapshot().state == DeviceState::Booting);
}

void test_ota_blocks_behaviors_and_returns_to_boot() {
    LifecycleService lifecycle;
    CHECK(lifecycle.finish_boot(true).ok());
    CHECK(lifecycle.begin_update().ok());
    CHECK(lifecycle.snapshot().state == DeviceState::Updating);
    CHECK(lifecycle.begin_behavior(Behavior::Happy).code() == ErrorCode::InvalidState);
    CHECK(lifecycle.enter_deep_sleep().code() == ErrorCode::InvalidState);
    CHECK(lifecycle.finish_update_and_reboot().ok());
    CHECK(lifecycle.snapshot().state == DeviceState::Booting);
}

}  // namespace

namespace plant::test {
int run_ota_power_service_tests();
int run_plant_application_tests();
}

int main() {
    test_happy_maps_to_three_outputs();
    test_duplicate_behavior_is_idempotent();
    test_wakeup_rejects_normal_interruption_but_accepts_sleep_request();
    test_stale_completion_is_ignored();
    test_sleep_completion_requests_sleeping_lifecycle();
    test_output_failure_enters_fault_and_safe_patterns();
    test_global_fault_is_accepted_while_idle();
    test_lifecycle_sleep_and_power_modes();
    test_ota_blocks_behaviors_and_returns_to_boot();
    failures += plant::test::run_ota_power_service_tests();
    failures += plant::test::run_plant_application_tests();

    if (failures != 0) {
        std::cerr << failures << " test checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All plant domain tests passed\n";
    return EXIT_SUCCESS;
}
