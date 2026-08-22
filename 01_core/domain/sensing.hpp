#pragma once

#include <cstdint>

namespace plant {

constexpr std::uint16_t kNormalizedSensorMaximum = 1000;

enum class PositionFeedbackState : std::uint8_t {
    Unavailable = 0,
    Valid,
    OpenCircuit,
    ShortCircuit,
    OutOfRange,
};

enum class MotionFault : std::uint8_t {
    None = 0,
    FeedbackInvalid,
    Stalled,
    OppositeDirection,
    Timeout,
};

struct PositionSnapshot {
    std::uint16_t actual_position{0};
    std::uint16_t target_position{0};
    PositionFeedbackState feedback{PositionFeedbackState::Unavailable};
    MotionFault fault{MotionFault::None};
    bool moving{false};
    bool target_reached{false};
};

struct AcousticSample {
    std::uint16_t volume_level{0};
    bool valid{false};
};

enum class AcousticState : std::uint8_t {
    Quiet = 0,
    Speaking,
    SustainedSpeech,
    SensorFault,
};

struct AcousticSnapshot {
    AcousticState state{AcousticState::Quiet};
    std::uint16_t volume_level{0};
    std::uint16_t noise_floor{0};
    std::uint32_t speaking_duration_ms{0};
    bool enabled{true};
    bool interference_masked{false};
};

struct IlluminationSample {
    std::uint16_t relative_level{0};
    bool valid{false};
};

enum class IlluminationState : std::uint8_t {
    Dark = 0,
    Ambient,
    BrightExposure,
    SensorFault,
};

struct IlluminationSnapshot {
    IlluminationState state{IlluminationState::Ambient};
    std::uint16_t relative_level{0};
    std::uint32_t bright_duration_ms{0};
    bool self_light_masked{false};
};

struct ClimateSample {
    std::int16_t temperature_centi_c{0};
    std::uint16_t relative_humidity_tenths_percent{0};
    bool valid{false};
};

enum class ClimateState : std::uint8_t {
    TooCold = 0,
    TooHot,
    TooDry,
    TooHumid,
    Suitable,
    SensorFault,
};

struct ClimateSnapshot {
    ClimateState state{ClimateState::SensorFault};
    std::int16_t temperature_centi_c{0};
    std::uint16_t relative_humidity_tenths_percent{0};
    std::uint32_t suitable_duration_ms{0};
    bool valid{false};
};

enum class BatteryState : std::uint8_t {
    Unavailable = 0,
    Normal,
    Low,
    Critical,
    SensorFault,
};

struct BatterySample {
    std::uint16_t voltage_mv{0};
    std::uint16_t level_per_mille{0};
    bool valid{false};
};

struct BatterySnapshot {
    BatteryState state{BatteryState::Unavailable};
    std::uint16_t voltage_mv{0};
    std::uint16_t level_per_mille{0};
    bool valid{false};
};

enum class GrowthSource : std::uint8_t {
    None = 0,
    Touch,
    SustainedSpeech,
    BrightExposure,
    SuitableClimate,
    InactivityDecay,
};

enum class GrowthAction : std::uint8_t {
    None = 0,
    MoveToPosition,
    ShowLimit,
};

struct GrowthDecision {
    GrowthAction action{GrowthAction::None};
    GrowthSource source{GrowthSource::None};
    std::uint16_t target_position{0};
};

struct GrowthSnapshot {
    GrowthSource recent_source{GrowthSource::None};
    GrowthSource pending_source{GrowthSource::None};
    std::uint8_t pending_count{0};
    bool pending{false};
    bool at_limit{false};
};

}  // namespace plant
