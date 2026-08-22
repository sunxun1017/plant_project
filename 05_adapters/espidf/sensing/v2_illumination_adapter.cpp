#include "05_adapters/espidf/sensing/v2_illumination_adapter.hpp"

#include <algorithm>

#include "06_bsp/plant_v2/plant_v2_board.hpp"

namespace plant {
namespace {

using Config = bsp::v2::BoardConfig;

std::uint16_t normalize_light(int raw) noexcept {
    const int bounded = std::clamp(
        raw,
        Config::Illumination::valid_raw_minimum,
        Config::Illumination::valid_raw_maximum);
    const std::uint16_t normalized = static_cast<std::uint16_t>(
        (bounded - Config::Illumination::valid_raw_minimum) *
        kNormalizedSensorMaximum /
        (Config::Illumination::valid_raw_maximum -
         Config::Illumination::valid_raw_minimum));
    return Config::Illumination::larger_raw_means_brighter
               ? normalized
               : static_cast<std::uint16_t>(kNormalizedSensorMaximum - normalized);
}

}  // namespace

V2IlluminationAdapter::V2IlluminationAdapter(V2AdcSampler& adc) noexcept : adc_(adc) {}

Status V2IlluminationAdapter::initialize() {
    initialized_ = true;
    next_sample_us_ = 0;
    return Status::success();
}

Status V2IlluminationAdapter::poll(
    std::uint64_t now_us,
    IlluminationSample& sample,
    bool& available) {
    sample = IlluminationSample{};
    available = false;
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (now_us < next_sample_us_) {
        return Status::success();
    }
    next_sample_us_ = now_us + Config::Illumination::sample_period_ms * 1000ULL;

    int raw = 0;
    const Status status = adc_.read_raw(
        static_cast<adc_channel_t>(Config::Adc::illumination_channel), raw);
    if (!status.ok()) {
        return status;
    }
    sample.valid = raw >= Config::Illumination::valid_raw_minimum &&
                   raw <= Config::Illumination::valid_raw_maximum;
    sample.relative_level = sample.valid ? normalize_light(raw) : 0;
    available = true;
    return Status::success();
}

}  // namespace plant
