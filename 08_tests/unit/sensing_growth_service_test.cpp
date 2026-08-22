#include <cstdint>

#include "03_services/growth/growth_service.hpp"
#include "03_services/sensing/acoustic_service.hpp"
#include "03_services/sensing/climate_service.hpp"
#include "03_services/sensing/illumination_service.hpp"

namespace plant::test {
namespace {

int failures = 0;

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

void test_growth_uses_measured_position_and_keeps_one_pending_credit() {
    GrowthConfig config{};
    config.step = 75;
    config.maximum_position = 900;
    config.pending_expiry_us = 1000;
    config.cooldown_us.fill(0);
    GrowthService service{config};

    CHECK_SENSING(service.submit(GrowthSource::Touch, 0).ok());
    CHECK_SENSING(service.submit(GrowthSource::SustainedSpeech, 1).code() == ErrorCode::Busy);
    GrowthDecision decision{};
    CHECK_SENSING(service.evaluate(
                       10,
                       false,
                       PositionSnapshot{400, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::None);
    CHECK_SENSING(service.evaluate(
                       20,
                       true,
                       PositionSnapshot{400, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::MoveToPosition);
    CHECK_SENSING(decision.target_position == 475);
    CHECK_SENSING(decision.source == GrowthSource::Touch);
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
                       PositionSnapshot{500, 0, PositionFeedbackState::Valid},
                       decision)
                       .ok());
    CHECK_SENSING(decision.action == GrowthAction::None);

    CHECK_SENSING(service.submit(GrowthSource::SuitableClimate, 400).ok());
    CHECK_SENSING(service.evaluate(
                       401,
                       true,
                       PositionSnapshot{500, 0, PositionFeedbackState::OpenCircuit},
                       decision)
                       .code() == ErrorCode::MotionFailure);
}

}  // namespace

int run_sensing_growth_service_tests() {
    test_acoustic_hysteresis_sustained_edge_and_mask();
    test_acoustic_invalid_and_disabled_do_not_create_activity();
    test_illumination_confirmation_interval_and_hysteresis();
    test_illumination_masks_rgb_self_light();
    test_climate_requires_both_values_and_uses_hysteresis();
    test_growth_uses_measured_position_and_keeps_one_pending_credit();
    test_growth_limit_expiry_cooldown_and_invalid_feedback();
    return failures;
}

}  // namespace plant::test
