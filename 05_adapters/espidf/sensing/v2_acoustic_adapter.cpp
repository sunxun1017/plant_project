#include "05_adapters/espidf/sensing/v2_acoustic_adapter.hpp"

#include <algorithm>

#include "06_bsp/plant_v2/plant_v2_board.hpp"
#include "driver/gpio.h"
#include "esp_timer.h"

namespace plant {
namespace {

using Config = bsp::v2::BoardConfig;

std::uint16_t normalize_envelope(int raw) noexcept {
    const int bounded = std::clamp(
        raw,
        Config::Microphone::valid_raw_minimum,
        Config::Microphone::valid_raw_maximum);
    return static_cast<std::uint16_t>(
        (bounded - Config::Microphone::valid_raw_minimum) *
        kNormalizedSensorMaximum /
        (Config::Microphone::valid_raw_maximum -
         Config::Microphone::valid_raw_minimum));
}

}  // namespace

V2AcousticAdapter::V2AcousticAdapter(V2AdcSampler& adc) noexcept : adc_(adc) {}

Status V2AcousticAdapter::initialize() {
    gpio_config_t power{};
    power.pin_bit_mask = 1ULL << Config::Gpio::microphone_power_enable;
    power.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&power) != ESP_OK) {
        return Status::failure(ErrorCode::SensorFailure);
    }
    initialized_ = true;
    return set_enabled(true);
}

Status V2AcousticAdapter::set_enabled(bool enabled) {
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (gpio_set_level(
            static_cast<gpio_num_t>(Config::Gpio::microphone_power_enable),
            enabled == Config::Microphone::power_enable_active_high ? 1 : 0) != ESP_OK) {
        return Status::failure(ErrorCode::SensorFailure);
    }
    enabled_ = enabled;
    next_sample_us_ = enabled
                          ? static_cast<std::uint64_t>(esp_timer_get_time()) +
                                Config::Microphone::front_end_settle_ms * 1000ULL
                          : 0;
    return Status::success();
}

Status V2AcousticAdapter::poll(
    std::uint64_t now_us,
    AcousticSample& sample,
    bool& available) {
    sample = AcousticSample{};
    available = false;
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (!enabled_ || now_us < next_sample_us_) {
        return Status::success();
    }
    next_sample_us_ = now_us + Config::Microphone::sample_period_ms * 1000ULL;

    int raw = 0;
    const Status status = adc_.read_raw(
        static_cast<adc_channel_t>(Config::Adc::microphone_channel), raw);
    if (!status.ok()) {
        return status;
    }
    sample.valid = raw >= Config::Microphone::valid_raw_minimum &&
                   raw <= Config::Microphone::valid_raw_maximum;
    sample.volume_level = sample.valid ? normalize_envelope(raw) : 0;
    available = true;
    return Status::success();
}

}  // namespace plant
