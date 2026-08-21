#pragma once

#include <cstdint>

#include "02_ports/light/light_port.hpp"

namespace plant {

class EspLedAdapter final : public ILightPort {
public:
    Status initialize();
    Status play(LightPattern pattern, std::uint32_t execution_id) override;
    Status stop() override;
    void tick(std::uint64_t now_us);

private:
    Status set_rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue);

    bool initialized_{false};
    LightPattern pattern_{LightPattern::FadeOut};
    std::uint64_t started_us_{0};
};

}  // namespace plant
