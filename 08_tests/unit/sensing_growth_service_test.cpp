#include <cstdint>

#include "03_services/growth/growth_service.hpp"
#include "03_services/lighting/light_arbitration_service.hpp"
#include "03_services/sensing/acoustic_service.hpp"
#include "03_services/sensing/battery_service.hpp"
#include "03_services/sensing/climate_service.hpp"
#include "03_services/sensing/illumination_service.hpp"

namespace plant::test {
namespace {

int failures = 0;

class FakeLightOutput final : public ILightPort {
public:
    Status play(LightPattern pattern, std::uint32_t) override {
        last_pattern = pattern;
        ++play_count;
        return Status::success();
    }
    Status stop() override {
        ++stop_count;
        return Status::success();
    }
    Status tick(std::uint64_t) override { return Status::success(); }
    Status set_intensity(std::uint16_t value) override {
        intensity = value;
        return Status::success();
    }

    LightPattern last_pattern{LightPattern::FadeOut};
    std::uint16_t intensity{0};
    int play_count{0};
    int stop_count{0};
};

#define CHECK_SENSING(condition) \
    do {                         \
        if (!(condition)) {      \
            ++failures;          \
        }                        \
    } while (false)

void test_acoustic_hysteresis_sustained_edge_and_mask() {
    AcousticDetectionConfig config{};
    config.initial_noise_floor = 100;
    config.speech_start_margin = 100;
    config.speech_stop_margin = 50;
    config.minimum_speech_us = 100;
    config.speech_end_hold_us = 100;
    config.sustained_window_us = 1000;
    config.sustained_required_us = 300;
    AcousticService service{config};

    CHECK_SENSING(!service.process(0, AcousticSample{300, true}, false));
    CHECK_SENSING(!service.process(100, AcousticSample{300, true}, false));
    CHECK_SENSING(service.snapshot().state == AcousticState::Speaking);
    CHECK_SENSING(!service.process(200, AcousticSample{300, true}, false));
    CHECK_SENSING(service.process(300, AcousticSample{300, true}, false));
    CHECK_SENSING(service.snapshot().state == AcousticState::SustainedSpeech);
    CHECK_SENSING(!service.process(400, AcousticSample{300, true}, false));

    CHECK_SENSING(!service.process(500, AcousticSample{300, true}, true));
    CHECK_SENSING(service.snapshot().state == AcousticState::Quiet);
    CHECK_SENSING(service.snapshot().interference_masked);
}

void test_acoustic_invalid_and_disabled_do_not_create_activity() {
    AcousticService service;
    CHECK_SENSING(!service.process(0, AcousticSample{0, false}, false));
    CHECK_SENSING(service.snapshot().state == AcousticState::SensorFault);
    service.set_enabled(false);
    CHECK_SENSING(!service.process(1000, AcousticSample{1000, true}, false));
    CHECK_SENSING(service.snapshot().state == AcousticState::Quiet);
    CHECK_SENSING(!service.snapshot().enabled);
}

void test_illumination_confirmation_interval_and_hysteresis() {
    IlluminationDetectionConfig config{};
    config.dark_threshold = 100;
    config.bright_enter_threshold = 700;
    config.bright_exit_threshold = 600;
    config.bright_confirm_us = 100;
    config.bright_exit_hold_us = 100;
    config.exposure_credit_interval_us = 200;
    IlluminationService service{config};

    CHECK_SENSING(!service.process(0, IlluminationSample{800, true}, false));
    CHECK_SENSING(!service.process(100, IlluminationSample{800, true}, false));
    CHECK_SENSING(service.snapshot().state == IlluminationState::BrightExposure);
    CHECK_SENSING(!service.process(299, IlluminationSample{800, true}, false));
    CHECK_SENSING(service.process(300, IlluminationSample{800, true}, false));
    CHECK_SENSING(!service.process(400, IlluminationSample{500, true}, false));
    CHECK_SENSING(!service.process(500, IlluminationSample{500, true}, false));
    CHECK_SENSING(service.snapshot().state == IlluminationState::Ambient);
}

void test_illumination_masks_rgb_self_light() {
    IlluminationDetectionConfig config{};
    config.bright_confirm_us = 100;
    config.exposure_credit_interval_us = 100;
    IlluminationService service{config};

    CHECK_SENSING(!service.process(0, IlluminationSample{900, true}, true));
    CHECK_SENSING(!service.process(1000, IlluminationSample{900, true}, true));
    CHECK_SENSING(service.snapshot().state != IlluminationState::BrightExposure);
}

void test_climate_requires_both_values_and_uses_hysteresis() {
    ClimateDetectionConfig config{};
    config.suitable_min_temperature_centi_c = 1800;
    config.suitable_max_temperature_centi_c = 3000;
    config.suitable_min_humidity_tenths_percent = 300;
    config.suitable_max_humidity_tenths_percent = 750;
    config.temperature_hysteresis_centi_c = 10;
    config.humidity_hysteresis_tenths_percent = 10;
    config.suitable_credit_interval_us = 200;
    ClimateService service{config};

    CHECK_SENSING(!service.process(0, ClimateSample{2000, 500, true}));
    CHECK_SENSING(service.snapshot().state == ClimateState::Suitable);
    CHECK_SENSING(!service.process(100, ClimateSample{1795, 500, true}));
    CHECK_SENSING(service.snapshot().state == ClimateState::Suitable);
    CHECK_SENSING(service.process(200, ClimateSample{2000, 500, true}));
    CHECK_SENSING(!service.process(300, ClimateSample{1780, 500, true}));
    CHECK_SENSING(service.snapshot().state == ClimateState::TooCold);
    CHECK_SENSING(!service.process(400, ClimateSample{2000, 800, true}));
    CHECK_SENSING(service.snapshot().state == ClimateState::TooHumid);
    CHECK_SENSING(!service.process(500, ClimateSample{2000, 500, false}));
    CHECK_SENSING(service.snapshot().state == ClimateState::SensorFault);
}

void test_each_valid_interaction_queues_a_bounded_growth_credit() {
    GrowthConfig config{};
    config.step = 75;
    config.maximum_position = 900;
    config.pending_expiry_us = 1000;
    config.cooldown_us.fill(0);
    GrowthService service{config};

    CHECK_SENSING(service.submit(GrowthSource::Touch, 0).ok());
    CHECK_SENSING(service.submit(GrowthSource::SustainedSpeech, 1).ok());
    GrowthDecision decision{};
    CHECK_SENSING(service.evaluate(
                       10,
                       false,
                       false,
                       PositionSnapshot{400, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::None);
    CHECK_SENSING(service.evaluate(
                       20,
                       true,
                       false,
                       PositionSnapshot{400, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::MoveToPosition);
    CHECK_SENSING(decision.target_position == 475);
    CHECK_SENSING(decision.source == GrowthSource::Touch);
    CHECK_SENSING(service.evaluate(
                       30,
                       true,
                       false,
                       PositionSnapshot{475, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::MoveToPosition);
    CHECK_SENSING(decision.target_position == 550);
    CHECK_SENSING(decision.source == GrowthSource::SustainedSpeech);
}

void test_growth_queue_rejects_only_the_credit_beyond_its_bound() {
    GrowthConfig config{};
    config.cooldown_us.fill(0);
    config.maximum_pending_credits = 4;
    config.pending_expiry_us = 0;
    GrowthService service{config};

    CHECK_SENSING(service.submit(GrowthSource::Touch, 1).ok());
    CHECK_SENSING(service.submit(GrowthSource::SustainedSpeech, 2).ok());
    CHECK_SENSING(service.submit(GrowthSource::BrightExposure, 3).ok());
    CHECK_SENSING(service.submit(GrowthSource::SuitableClimate, 4).ok());
    CHECK_SENSING(
        service.submit(GrowthSource::Touch, 5).code() == ErrorCode::Busy);
    CHECK_SENSING(service.snapshot().pending);
    CHECK_SENSING(service.snapshot().pending_source == GrowthSource::Touch);
    CHECK_SENSING(service.snapshot().pending_count == 4);
}

void test_growth_limit_expiry_cooldown_and_invalid_feedback() {
    GrowthConfig config{};
    config.maximum_position = 900;
    config.limit_tolerance = 10;
    config.pending_expiry_us = 100;
    config.cooldown_us.fill(0);
    config.cooldown_us[static_cast<std::size_t>(GrowthSource::Touch)] = 500;
    GrowthService service{config};
    GrowthDecision decision{};

    CHECK_SENSING(service.submit(GrowthSource::Touch, 0).ok());
    CHECK_SENSING(service.evaluate(
                       10,
                       true,
                       false,
                       PositionSnapshot{895, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::ShowLimit);
    CHECK_SENSING(service.snapshot().at_limit);
    CHECK_SENSING(service.submit(GrowthSource::Touch, 100).code() == ErrorCode::Busy);

    CHECK_SENSING(service.submit(GrowthSource::BrightExposure, 200).ok());
    CHECK_SENSING(service.evaluate(
                       300,
                       true,
                       false,
                       PositionSnapshot{500, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::None);

    CHECK_SENSING(service.submit(GrowthSource::SuitableClimate, 400).ok());
    CHECK_SENSING(service.evaluate(
                       401,
                       true,
                       false,
                       PositionSnapshot{500, 0, PositionFeedbackState::OpenCircuit},
                       decision)
                       .code() == ErrorCode::MotionFailure);
}

void test_inactivity_decays_slowly_and_interaction_resets_the_timer() {
    GrowthConfig config{};
    config.step = 50;
    config.minimum_position = 100;
    config.decay_step = 25;
    config.inactivity_before_decay_us = 1000;
    config.decay_interval_us = 500;
    config.cooldown_us.fill(0);
    GrowthService service{config};
    GrowthDecision decision{};

    CHECK_SENSING(service.evaluate(
                       0,
                       false,
                       true,
                       PositionSnapshot{400, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::None);
    CHECK_SENSING(service.evaluate(
                       1000,
                       false,
                       true,
                       PositionSnapshot{400, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.source == GrowthSource::InactivityDecay);
    CHECK_SENSING(decision.target_position == 375);

    CHECK_SENSING(service.evaluate(
                       1500,
                       false,
                       true,
                       PositionSnapshot{375, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.target_position == 350);

    CHECK_SENSING(service.submit(GrowthSource::Touch, 1600).ok());
    CHECK_SENSING(service.evaluate(
                       1601,
                       true,
                       true,
                       PositionSnapshot{350, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.source == GrowthSource::Touch);
    CHECK_SENSING(decision.target_position == 400);
    CHECK_SENSING(service.evaluate(
                       2599,
                       false,
                       true,
                       PositionSnapshot{400, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::None);
}

void test_battery_levels_and_sensor_fault() {
    BatteryService service{200, 80};
    service.process(BatterySample{3900, 750, true});
    CHECK_SENSING(service.snapshot().state == BatteryState::Normal);
    service.process(BatterySample{3600, 200, true});
    CHECK_SENSING(service.snapshot().state == BatteryState::Low);
    service.process(BatterySample{3400, 80, true});
    CHECK_SENSING(service.snapshot().state == BatteryState::Critical);
    service.process(BatterySample{0, 0, false});
    CHECK_SENSING(service.snapshot().state == BatteryState::SensorFault);
}

void test_light_priority_and_expiry_restore_background() {
    FakeLightOutput output;
    LightArbitrationService service{output};
    CHECK_SENSING(service.request(
                       LightRequestSource::Sunlight,
                       LightPattern::SunGlow,
                       500,
                       0)
                       .ok());
    CHECK_SENSING(service.tick(0).ok());
    CHECK_SENSING(output.last_pattern == LightPattern::SunGlow);

    CHECK_SENSING(service.request(
                       LightRequestSource::Speech,
                       LightPattern::ListeningBreath,
                       800,
                       10,
                       100)
                       .ok());
    CHECK_SENSING(service.tick(10).ok());
    CHECK_SENSING(output.last_pattern == LightPattern::ListeningBreath);
    CHECK_SENSING(output.intensity == 800);
    CHECK_SENSING(!service.interferes_with_illumination());
    CHECK_SENSING(service.request(
                       LightRequestSource::Growth,
                       LightPattern::GrowthRise,
                       1000,
                       20,
                       50)
                       .ok());
    CHECK_SENSING(service.tick(20).ok());
    CHECK_SENSING(output.last_pattern == LightPattern::GrowthRise);
    CHECK_SENSING(service.interferes_with_illumination());
    CHECK_SENSING(service.tick(80).ok());
    CHECK_SENSING(output.last_pattern == LightPattern::ListeningBreath);
    CHECK_SENSING(service.tick(120).ok());
    CHECK_SENSING(output.last_pattern == LightPattern::SunGlow);
}

}  // namespace

int run_sensing_growth_service_tests() {
    test_acoustic_hysteresis_sustained_edge_and_mask();
    test_acoustic_invalid_and_disabled_do_not_create_activity();
    test_illumination_confirmation_interval_and_hysteresis();
    test_illumination_masks_rgb_self_light();
    test_climate_requires_both_values_and_uses_hysteresis();
    test_each_valid_interaction_queues_a_bounded_growth_credit();
    test_growth_queue_rejects_only_the_credit_beyond_its_bound();
    test_growth_limit_expiry_cooldown_and_invalid_feedback();
    test_inactivity_decays_slowly_and_interaction_resets_the_timer();
    test_battery_levels_and_sensor_fault();
    test_light_priority_and_expiry_restore_background();
    return failures;
}

}  // namespace plant::test
