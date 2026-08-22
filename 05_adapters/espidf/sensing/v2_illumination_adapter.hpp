#pragma once

#include <cstdint>

#include "02_ports/sensing/illumination_sensor_port.hpp"
#include "05_adapters/espidf/sensing/v2_adc_sampler.hpp"

namespace plant {

class V2IlluminationAdapter final : public IIlluminationSensorPort {
public:
    explicit V2IlluminationAdapter(V2AdcSampler& adc) noexcept;

    Status initialize() override;
    Status poll(
        std::uint64_t now_us,
        IlluminationSample& sample,
        bool& available) override;

private:
    V2AdcSampler& adc_;
    std::uint64_t next_sample_us_{0};
    bool initialized_{false};
};

}  // namespace plant
