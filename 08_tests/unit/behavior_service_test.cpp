#include <cstdlib>
#include <iostream>

#include "03_services/behavior/behavior_policy.hpp"
#include "03_services/behavior/behavior_service.hpp"
#include "03_services/lifecycle/lifecycle_service.hpp"

namespace {

using namespace plant;

class FakeMotion final : public IMotionPort {
public:
    Status stop() override {
        ++stop_count;
        return stop_status;
    }

    int stop_count{0};
    Status stop_status{};
};

class FakeLight final : public ILightPort {
public:
    Status play(LightCue cue, std::uint32_t execution_id) override {
        last_cue = cue;
        last_execution_id = execution_id;
        ++play_count;
        return next_status;
    }

    Status stop() override {
        ++stop_count;
        return stop_status;
    }

    Status tick(std::uint64_t) override { return tick_status; }

    LightCue last_cue{LightCue::Happy};
    std::uint32_t last_execution_id{0};
    int play_count{0};
    int stop_count{0};
    Status stop_status{};
    Status next_status{};
    Status tick_status{};
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
        return stop_status;
    }

    Status tick(std::uint64_t) override { return tick_status; }
    bool active() const noexcept override { return output_active; }

    HapticPattern last_pattern{HapticPattern::Off};
    std::uint32_t last_execution_id{0};
    int play_count{0};
    int stop_count{0};
    Status stop_status{};
    Status next_status{};
    Status tick_status{};
    bool output_active{false};
};

int failures = 0;

#define CHECK(condition)                                                                     \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            std::cerr << __FILE__ << ':' << __LINE__ << " CHECK failed: " #condition << '\n'; \
            ++failures;                                                                      \
        }                                                                                    \
    } while (false)

void test_stop_after_semantic_completion_stops_every_output() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};
    CHECK(service.start(Behavior::Happy).ok());
    CHECK(service.take_outcome() == BehaviorOutcome::CompletedIdle);
    CHECK(service.stop().ok());
    CHECK(motion.stop_count == 2);
    CHECK(light.stop_count == 1);
    CHECK(haptic.stop_count == 1);
    CHECK(service.take_outcome() == BehaviorOutcome::Stopped);
}

void test_stop_failure_attempts_all_outputs_without_restarting_haptic() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};
    CHECK(service.start(Behavior::Happy).ok());
    motion.stop_status = Status::failure(ErrorCode::MotionFailure);
    CHECK(service.stop().code() == ErrorCode::MotionFailure);
    CHECK(light.stop_count == 1);
    CHECK(haptic.stop_count == 1);
    CHECK(haptic.play_count == 1);
    CHECK(service.snapshot().state == BehaviorRunState::Fault);
    CHECK(service.take_outcome() == BehaviorOutcome::Faulted);
}

void test_happy_maps_to_three_outputs() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Happy).ok());
    CHECK(service.snapshot().state == BehaviorRunState::Idle);
    CHECK(light.last_cue == LightCue::Happy);
    CHECK(haptic.last_pattern == HapticPattern::DoubleSoftPulse);
    CHECK(light.last_execution_id == haptic.last_execution_id);
}

void test_behavior_starts_without_a_previous_behavior() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.snapshot().behavior == Behavior::None);
    CHECK(service.start(Behavior::Happy).ok());
    CHECK(service.snapshot().behavior == Behavior::Happy);
}

void test_growth_only_profile_keeps_servo_stopped_for_regular_behaviors() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic, BehaviorExecutionConfig{5000000}};

    CHECK(service.start(Behavior::Happy).ok());
    CHECK(motion.stop_count == 1);
    CHECK(service.snapshot().state == BehaviorRunState::Idle);
    CHECK(service.take_outcome() == BehaviorOutcome::CompletedIdle);
    CHECK(light.last_cue == LightCue::Happy);
    CHECK(haptic.last_pattern == HapticPattern::DoubleSoftPulse);

    CHECK(service.start(Behavior::Sleep, InterruptionReason::WakeSleep).ok());
    CHECK(motion.stop_count == 2);
    CHECK(service.take_outcome() == BehaviorOutcome::CompletedSleeping);
}

void test_duplicate_behavior_is_idempotent() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Happy).ok());
    const auto first_id = service.snapshot().execution_id;
    CHECK(service.start(Behavior::Happy).ok());
    CHECK(service.snapshot().execution_id != first_id);
    CHECK(motion.stop_count == 2);
}

void test_wakeup_and_sleep_requests_are_independent_v2_behaviors() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::WakeUp).ok());
    CHECK(service.start(Behavior::Happy).ok());
    CHECK(service.start(Behavior::Sleep, InterruptionReason::WakeSleep).ok());
    CHECK(service.snapshot().behavior == Behavior::Sleep);
}

void test_sleep_completion_requests_sleeping_lifecycle() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Sleep).ok());
    CHECK(service.take_outcome() == BehaviorOutcome::CompletedSleeping);
}

void test_wakeup_keeps_its_own_light_cue() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::WakeUp).ok());
    CHECK(service.snapshot().state == BehaviorRunState::Idle);
    CHECK(light.last_cue == LightCue::WakeUp);
    CHECK(light.play_count == 1);
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
    CHECK(motion.stop_count == 2);
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
    CHECK(motion.stop_count == 1);
    CHECK(light.last_cue == LightCue::Error);
}

void test_runtime_light_failure_enters_fault() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Happy).ok());
    light.tick_status = Status::failure(ErrorCode::LightingFailure);
    CHECK(service.tick(1000).code() == ErrorCode::LightingFailure);
    CHECK(service.snapshot().state == BehaviorRunState::Fault);
    CHECK(motion.stop_count == 2);
}

void test_runtime_haptic_failure_enters_fault() {
    FakeMotion motion;
    FakeLight light;
    FakeHaptic haptic;
    BehaviorService service{motion, light, haptic};

    CHECK(service.start(Behavior::Happy).ok());
    haptic.tick_status = Status::failure(ErrorCode::HapticFailure);
    CHECK(service.tick(1000).code() == ErrorCode::HapticFailure);
    CHECK(service.snapshot().state == BehaviorRunState::Fault);
    CHECK(haptic.stop_count == 1);
}

void test_lifecycle_sleep_and_power_modes() {
    LifecycleService lifecycle;
    CHECK(lifecycle.snapshot().state == DeviceState::Booting);
    CHECK(lifecycle.finish_boot(true).ok());
    CHECK(lifecycle.begin_behavior(Behavior::Sleep).ok());
    CHECK(lifecycle.apply_behavior_outcome(BehaviorOutcome::CompletedSleeping).ok());
    CHECK(lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK(lifecycle.enter_light_sleep_mode().ok());
    CHECK(lifecycle.snapshot().power_mode == PowerMode::LightSleep);
    CHECK(lifecycle.enter_deep_sleep_mode().ok());
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
    CHECK(lifecycle.enter_deep_sleep_mode().code() == ErrorCode::InvalidState);
    CHECK(lifecycle.finish_update_and_reboot().ok());
    CHECK(lifecycle.snapshot().state == DeviceState::Booting);
}

}  // namespace

namespace plant::test {
int run_ota_power_service_tests();
int run_plant_v2_application_tests();
int run_protocol_codec_tests();
int run_sensing_growth_service_tests();
}

int main() {
    test_stop_after_semantic_completion_stops_every_output();
    test_stop_failure_attempts_all_outputs_without_restarting_haptic();
    test_happy_maps_to_three_outputs();
    test_behavior_starts_without_a_previous_behavior();
    test_growth_only_profile_keeps_servo_stopped_for_regular_behaviors();
    test_duplicate_behavior_is_idempotent();
    test_wakeup_and_sleep_requests_are_independent_v2_behaviors();
    test_sleep_completion_requests_sleeping_lifecycle();
    test_wakeup_keeps_its_own_light_cue();
    test_output_failure_enters_fault_and_safe_patterns();
    test_global_fault_is_accepted_while_idle();
    test_runtime_light_failure_enters_fault();
    test_runtime_haptic_failure_enters_fault();
    test_lifecycle_sleep_and_power_modes();
    test_ota_blocks_behaviors_and_returns_to_boot();
    failures += plant::test::run_ota_power_service_tests();
    failures += plant::test::run_plant_v2_application_tests();
    failures += plant::test::run_protocol_codec_tests();
    failures += plant::test::run_sensing_growth_service_tests();

    if (failures != 0) {
        std::cerr << failures << " test checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All plant domain tests passed\n";
    return EXIT_SUCCESS;
}
