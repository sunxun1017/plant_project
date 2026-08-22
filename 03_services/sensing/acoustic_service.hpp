#pragma once

#include <cstdint>

#include "01_core/domain/configuration.hpp"
#include "01_core/domain/sensing.hpp"

namespace plant {

class AcousticService {
public:
    explicit AcousticService(AcousticDetectionConfig config = {}) noexcept;

    void set_enabled(bool enabled) noexcept;
    // 返回 true 只表示本轮第一次进入 SustainedSpeech，可用于提交一次促进量。
    bool process(
        std::uint64_t now_us,
        const AcousticSample& sample,
        bool interference_masked) noexcept;
    [[nodiscard]] AcousticSnapshot snapshot() const noexcept;

private:
    void reset_activity() noexcept;
    void enter_quiet() noexcept;
    [[nodiscard]] std::uint16_t start_threshold() const noexcept;
    [[nodiscard]] std::uint16_t stop_threshold() const noexcept;

    AcousticDetectionConfig config_;
    AcousticSnapshot snapshot_{};
    std::uint64_t last_sample_us_{0};
    std::uint64_t start_candidate_us_{0};
    std::uint64_t below_stop_since_us_{0};
    std::uint64_t window_started_us_{0};
    std::uint64_t speaking_accumulated_us_{0};
    bool time_initialized_{false};
    bool start_candidate_active_{false};
    bool below_stop_active_{false};
};

}  // namespace plant
