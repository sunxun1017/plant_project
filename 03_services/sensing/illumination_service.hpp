#pragma once

#include <cstdint>

#include "01_core/domain/configuration.hpp"
#include "01_core/domain/sensing.hpp"

namespace plant {

class IlluminationService {
public:
    explicit IlluminationService(IlluminationDetectionConfig config = {}) noexcept;

    // 返回 true 表示本轮完成一个有效明亮照射时间片。
    bool process(
        std::uint64_t now_us,
        const IlluminationSample& sample,
        bool self_light_masked) noexcept;
    void reset_exposure() noexcept;
    [[nodiscard]] IlluminationSnapshot snapshot() const noexcept;

private:
    [[nodiscard]] IlluminationState non_bright_state(std::uint16_t level) const noexcept;

    IlluminationDetectionConfig config_;
    IlluminationSnapshot snapshot_{};
    std::uint64_t bright_candidate_since_us_{0};
    std::uint64_t exit_candidate_since_us_{0};
    std::uint64_t exposure_started_us_{0};
    std::uint64_t last_credit_us_{0};
    bool bright_candidate_active_{false};
    bool exit_candidate_active_{false};
};

}  // namespace plant
