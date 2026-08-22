#pragma once

#include "01_core/domain/sensing.hpp"

namespace plant {

class BatteryService {
public:
    BatteryService(
        std::uint16_t low_level_per_mille,
        std::uint16_t critical_level_per_mille) noexcept;

    void process(const BatterySample& sample) noexcept;
    [[nodiscard]] BatterySnapshot snapshot() const noexcept;

private:
    std::uint16_t low_level_per_mille_;
    std::uint16_t critical_level_per_mille_;
    BatterySnapshot snapshot_{};
};

}  // namespace plant
