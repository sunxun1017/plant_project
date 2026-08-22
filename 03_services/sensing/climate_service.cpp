#include "03_services/sensing/climate_service.hpp"

#include <algorithm>

namespace plant {

ClimateService::ClimateService(ClimateDetectionConfig config) noexcept : config_(config) {}

bool ClimateService::process(std::uint64_t now_us, const ClimateSample& sample) noexcept {
    snapshot_.temperature_centi_c = sample.temperature_centi_c;
    snapshot_.relative_humidity_tenths_percent = sample.relative_humidity_tenths_percent;
    snapshot_.valid = sample.valid && sample.temperature_centi_c >= -4000 &&
                      sample.temperature_centi_c <= 8500 &&
                      sample.relative_humidity_tenths_percent <= 1000;
    if (!snapshot_.valid) {
        snapshot_.state = ClimateState::SensorFault;
        reset_suitable_time();
        return false;
    }

    const ClimateState next = classify(sample);
    if (next != ClimateState::Suitable) {
        snapshot_.state = next;
        reset_suitable_time();
        return false;
    }

    if (snapshot_.state != ClimateState::Suitable) {
        suitable_started_us_ = now_us;
        last_credit_us_ = now_us;
    }
    snapshot_.state = ClimateState::Suitable;
    snapshot_.suitable_duration_ms = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        (now_us - suitable_started_us_) / 1000ULL, UINT32_MAX));
    if (config_.suitable_credit_interval_us != 0 &&
        now_us - last_credit_us_ >= config_.suitable_credit_interval_us) {
        last_credit_us_ = now_us;
        return true;
    }
    return false;
}

void ClimateService::reset_suitable_time() noexcept {
    snapshot_.suitable_duration_ms = 0;
    suitable_started_us_ = 0;
    last_credit_us_ = 0;
}

ClimateSnapshot ClimateService::snapshot() const noexcept {
    return snapshot_;
}

ClimateState ClimateService::classify(const ClimateSample& sample) const noexcept {
    const bool was_suitable = snapshot_.state == ClimateState::Suitable;
    const std::int32_t temperature_margin =
        was_suitable ? config_.temperature_hysteresis_centi_c
                     : -config_.temperature_hysteresis_centi_c;
    const std::int32_t humidity_margin =
        was_suitable ? static_cast<std::int32_t>(config_.humidity_hysteresis_tenths_percent)
                     : -static_cast<std::int32_t>(config_.humidity_hysteresis_tenths_percent);

    const std::int32_t minimum_temperature =
        config_.suitable_min_temperature_centi_c - temperature_margin;
    const std::int32_t maximum_temperature =
        config_.suitable_max_temperature_centi_c + temperature_margin;
    const std::int32_t minimum_humidity =
        config_.suitable_min_humidity_tenths_percent - humidity_margin;
    const std::int32_t maximum_humidity =
        config_.suitable_max_humidity_tenths_percent + humidity_margin;

    if (sample.temperature_centi_c < minimum_temperature) {
        return ClimateState::TooCold;
    }
    if (sample.temperature_centi_c > maximum_temperature) {
        return ClimateState::TooHot;
    }
    if (sample.relative_humidity_tenths_percent < minimum_humidity) {
        return ClimateState::TooDry;
    }
    if (sample.relative_humidity_tenths_percent > maximum_humidity) {
        return ClimateState::TooHumid;
    }
    return ClimateState::Suitable;
}

}  // namespace plant
