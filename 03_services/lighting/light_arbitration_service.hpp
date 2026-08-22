#pragma once

#include <array>
#include <cstdint>

#include "02_ports/light/light_port.hpp"

namespace plant {

enum class LightRequestSource : std::uint8_t {
    Sunlight = 0,
    Climate,
    Speech,
    Touch,
    ForegroundBehavior,
    Growth,
    Error,
};

class LightArbitrationService final : public ILightPort {
public:
    explicit LightArbitrationService(ILightPort& output) noexcept;

    Status play(LightPattern pattern, std::uint32_t execution_id) override;
    Status stop() override;
    Status tick(std::uint64_t now_us) override;
    Status set_intensity(std::uint16_t intensity) override;

    Status request(
        LightRequestSource source,
        LightPattern pattern,
        std::uint16_t intensity,
        std::uint64_t now_us,
        std::uint64_t duration_us = 0);
    void clear(LightRequestSource source) noexcept;
    [[nodiscard]] bool emitting() const noexcept;
    [[nodiscard]] LightPattern active_pattern() const noexcept;
    [[nodiscard]] bool interferes_with_illumination() const noexcept;

private:
    struct Request {
        LightPattern pattern{LightPattern::FadeOut};
        std::uint16_t intensity{1000};
        std::uint64_t expires_us{0};
        bool active{false};
    };

    [[nodiscard]] static std::size_t index(LightRequestSource source) noexcept;
    [[nodiscard]] static std::uint8_t priority(LightRequestSource source) noexcept;

    ILightPort& output_;
    std::array<Request, 7> requests_{};
    LightPattern active_pattern_{LightPattern::FadeOut};
    LightRequestSource active_source_{LightRequestSource::Sunlight};
    std::uint64_t last_tick_us_{0};
    std::uint16_t foreground_intensity_{1000};
    bool output_active_{false};
};

}  // namespace plant
