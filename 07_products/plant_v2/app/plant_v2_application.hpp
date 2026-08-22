#pragma once

#include <cstdint>

#include "01_core/domain/sensing.hpp"
#include "02_ports/motion/position_motion_port.hpp"
#include "02_ports/sensing/acoustic_sensor_port.hpp"
#include "02_ports/sensing/battery_sensor_port.hpp"
#include "02_ports/sensing/climate_sensor_port.hpp"
#include "02_ports/sensing/illumination_sensor_port.hpp"
#include "03_services/growth/growth_service.hpp"
#include "03_services/lighting/light_arbitration_service.hpp"
#include "03_services/sensing/acoustic_service.hpp"
#include "03_services/sensing/battery_service.hpp"
#include "03_services/sensing/climate_service.hpp"
#include "03_services/sensing/illumination_service.hpp"
#include "07_products/plant_v1/app/plant_application.hpp"

namespace plant {

class PlantV2Application {
public:
    PlantV2Application(
        PlantApplication& base,
        LifecycleService& lifecycle,
        BehaviorService& behavior,
        OtaService& ota,
        IPositionMotionPort& motion,
        IAcousticSensorPort& acoustic_port,
        IIlluminationSensorPort& illumination_port,
        IClimateSensorPort& climate_port,
        IBatterySensorPort& battery_port,
        AcousticService& acoustic,
        IlluminationService& illumination,
        ClimateService& climate,
        BatteryService& battery,
        GrowthService& growth,
        LightArbitrationService& light,
        std::uint64_t actuator_recovery_us = 0) noexcept;

    Status tick(std::uint64_t now_us);
    Status handle_touch(TouchGesture gesture, std::uint64_t now_us);
    Status handle_command(const Command& command);
    Status handle_communication_connected();
    Status handle_communication_disconnected();
    Status handle_idle_timeout();

    [[nodiscard]] bool growth_motion_active() const noexcept;
    [[nodiscard]] Behavior growth_motion_behavior() const noexcept;
    [[nodiscard]] PositionSnapshot position_snapshot() const noexcept;
    [[nodiscard]] AcousticSnapshot acoustic_snapshot() const noexcept;
    [[nodiscard]] IlluminationSnapshot illumination_snapshot() const noexcept;
    [[nodiscard]] ClimateSnapshot climate_snapshot() const noexcept;
    [[nodiscard]] BatterySnapshot battery_snapshot() const noexcept;
    [[nodiscard]] GrowthSnapshot growth_snapshot() const noexcept;
    [[nodiscard]] bool boot_sensors_ready() const noexcept;
    [[nodiscard]] bool boot_critical_sensors_ok() const noexcept;

private:
    Status poll_motion(std::uint64_t now_us);
    Status poll_sensors(std::uint64_t now_us, bool actuator_interference);
    Status evaluate_growth(std::uint64_t now_us);
    Status enter_fault(Status cause);
    void update_sleep_sampling();
    [[nodiscard]] std::uint32_t next_growth_execution_id() noexcept;

    PlantApplication& base_;
    LifecycleService& lifecycle_;
    BehaviorService& behavior_;
    OtaService& ota_;
    IPositionMotionPort& motion_;
    IAcousticSensorPort& acoustic_port_;
    IIlluminationSensorPort& illumination_port_;
    IClimateSensorPort& climate_port_;
    IBatterySensorPort& battery_port_;
    AcousticService& acoustic_;
    IlluminationService& illumination_;
    ClimateService& climate_;
    BatteryService& battery_;
    GrowthService& growth_;
    LightArbitrationService& light_;
    std::uint32_t growth_execution_id_{0};
    bool growth_motion_active_{false};
    GrowthSource growth_motion_source_{GrowthSource::None};
    bool return_to_sleep_after_decay_{false};
    std::uint64_t actuator_recovery_us_{0};
    std::uint64_t actuator_interference_until_us_{0};
    bool acoustic_sampling_enabled_{true};
    bool acoustic_seen_{false};
    bool illumination_seen_{false};
    bool climate_seen_{false};
    bool battery_seen_{false};
};

}  // namespace plant
