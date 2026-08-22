#pragma once

#include <cstdint>

#include "02_ports/light/light_port.hpp"

namespace plant {

struct EspLedConfig {
    int red_gpio;
    int green_gpio;
    int blue_gpio;
    std::uint32_t frequency_hz;
    std::uint8_t duty_resolution_bits;
    std::uint8_t maximum_duty;
    bool active_high;
};

class EspLedAdapter final : public ILightPort {
public:
    EspLedAdapter() noexcept;
    explicit EspLedAdapter(EspLedConfig config) noexcept;
    Status initialize();
    Status play(LightPattern pattern, std::uint32_t execution_id) override;
    Status stop() override;
    Status tick(std::uint64_t now_us) override;
    Status set_intensity(std::uint16_t intensity) override;

private:
    Status set_rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue);

    EspLedConfig config_;
    bool initialized_{false};
    LightPattern pattern_{LightPattern::FadeOut};
    std::uint64_t started_us_{0};
    std::uint16_t intensity_{1000};
};

}  // namespace plant
