#include "05_adapters/espidf/sensing/v2_adc_sampler.hpp"

#include "06_bsp/plant_v2/plant_v2_board.hpp"

namespace plant {

Status V2AdcSampler::initialize() {
    using Config = bsp::v2::BoardConfig;
    if (initialized_) {
        return Status::success();
    }

    adc_oneshot_unit_init_cfg_t unit_config{};
    unit_config.unit_id = ADC_UNIT_1;
    unit_config.ulp_mode = ADC_ULP_MODE_DISABLE;
    if (adc_oneshot_new_unit(&unit_config, &unit_) != ESP_OK) {
        return Status::failure(ErrorCode::SensorFailure);
    }

    adc_oneshot_chan_cfg_t channel_config{};
    channel_config.atten = ADC_ATTEN_DB_12;
    channel_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    for (const int channel : Config::Adc::channels) {
        if (adc_oneshot_config_channel(
                unit_, static_cast<adc_channel_t>(channel), &channel_config) != ESP_OK) {
            (void)adc_oneshot_del_unit(unit_);
            unit_ = nullptr;
            return Status::failure(ErrorCode::SensorFailure);
        }
    }
    initialized_ = true;
    return Status::success();
}

Status V2AdcSampler::read_raw(adc_channel_t channel, int& raw_value) const {
    if (!initialized_ || unit_ == nullptr) {
        return Status::failure(ErrorCode::InvalidState);
    }
    return adc_oneshot_read(unit_, channel, &raw_value) == ESP_OK
               ? Status::success()
               : Status::failure(ErrorCode::SensorFailure);
}

}  // namespace plant
