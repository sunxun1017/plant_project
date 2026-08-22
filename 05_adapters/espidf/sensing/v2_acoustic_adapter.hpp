#pragma once

#include <cstdint>

#include "02_ports/sensing/acoustic_sensor_port.hpp"
#include "05_adapters/espidf/sensing/v2_adc_sampler.hpp"

namespace plant {

class V2AcousticAdapter final : public IAcousticSensorPort {
public:
    explicit V2AcousticAdapter(V2AdcSampler& adc) noexcept;

    Status initialize() override;
    Status set_enabled(bool enabled) override;
    Status poll(
        std::uint64_t now_us,
        AcousticSample& sample,
        bool& available) override;

private:
    V2AdcSampler& adc_;
    std::uint64_t next_sample_us_{0};
    bool initialized_{false};
    bool enabled_{false};
};

}  // namespace plant
