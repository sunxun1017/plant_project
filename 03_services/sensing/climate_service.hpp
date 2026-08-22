#pragma once

#include <cstdint>

#include "01_core/domain/configuration.hpp"
#include "01_core/domain/sensing.hpp"

namespace plant {

class ClimateService {
public:
    explicit ClimateService(ClimateDetectionConfig config = {}) noexcept;

    // 返回 true 表示温度和湿度同时适宜满一个有效时间片。
    bool process(std::uint64_t now_us, const ClimateSample& sample) noexcept;
    void reset_suitable_time() noexcept;
    [[nodiscard]] ClimateSnapshot snapshot() const noexcept;

private:
    [[nodiscard]] ClimateState classify(const ClimateSample& sample) const noexcept;

    ClimateDetectionConfig config_;
    ClimateSnapshot snapshot_{};
    std::uint64_t suitable_started_us_{0};
    std::uint64_t last_credit_us_{0};
};

}  // namespace plant
