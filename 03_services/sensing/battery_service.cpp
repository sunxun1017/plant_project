#include "03_services/sensing/battery_service.hpp"

namespace plant {

BatteryService::BatteryService(
    std::uint16_t low_level_per_mille,
    std::uint16_t critical_level_per_mille) noexcept
    : low_level_per_mille_(low_level_per_mille),
      critical_level_per_mille_(critical_level_per_mille) {}

void BatteryService::process(const BatterySample& sample) noexcept {
    snapshot_.voltage_mv = sample.voltage_mv;
    snapshot_.level_per_mille = sample.level_per_mille;
    snapshot_.valid = sample.valid && sample.level_per_mille <= kNormalizedSensorMaximum;
    if (!snapshot_.valid) {
        snapshot_.state = BatteryState::SensorFault;
    } else if (sample.level_per_mille <= critical_level_per_mille_) {
        snapshot_.state = BatteryState::Critical;
    } else if (sample.level_per_mille <= low_level_per_mille_) {
        snapshot_.state = BatteryState::Low;
    } else {
        snapshot_.state = BatteryState::Normal;
    }
}

BatterySnapshot BatteryService::snapshot() const noexcept {
    return snapshot_;
}

}  // namespace plant
