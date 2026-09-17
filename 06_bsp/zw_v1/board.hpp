#pragma once

#include <cstdint>

namespace plant::bsp::zw_v1 {
// Source: ZW V1.0(1).pdf, checked against Pads6/Nets6 in release ZW.PcbDoc.
// GPIO numbers, NOT U1 package pad numbers. GPIO0/1 belong to X2.
struct Board final {
    static constexpr int i2c_sda = 7;
    static constexpr int i2c_scl = 10;
    static constexpr int expander_interrupt = 2;
    static constexpr int servo_feedback = 3;  // ADC1 channel 3, through R7
    static constexpr int servo_pwm = 21;     // also ROM UART0 TX: external isolation needed
    static constexpr int audio_mclk = 20;
    static constexpr int audio_bclk = 4;
    static constexpr int audio_lrck = 5;
    static constexpr int audio_din = 6;
    static constexpr std::uint32_t i2c_hz = 100000;
    static constexpr std::uint8_t expander_address = 0x20; // A0/A1/A2 grounded
    static constexpr std::uint8_t climate_address = 0x44; // GXHT40-AD
    static constexpr std::uint8_t charger_address = 0x6b; // BQ24259
    static constexpr std::uint8_t codec_address = 0x18; // ES8311 CE grounded
};

enum class Output : std::uint8_t {
    Vibration = 1, Radar = 2, Servo = 3, PeripheralRail = 4, Audio = 6, Charge = 7,
};
constexpr std::uint8_t bit(unsigned index) { return static_cast<std::uint8_t>(1U << index); }
// All six outputs drive PNP stages: writing LOW enables the corresponding load.
// P0 is physically grounded. Keep its latch LOW (avoid its weak source current).
// P5 is the TTP223 input: ALWAYS release it by writing HIGH.
constexpr std::uint8_t input_mask = bit(5);
constexpr std::uint8_t safe_latch = 0xfe;
constexpr std::uint8_t rail_on_latch = safe_latch & ~bit(4);
// Keep ES8311 digital supply on too: AVDD and I2C stay live on this PCB.
// Software standby is used until the board isolates the whole codec domain.
constexpr std::uint8_t standby_latch = rail_on_latch & ~bit(6);
constexpr bool touch_pressed(std::uint8_t port) { return (port & input_mask) == 0; }
constexpr std::uint8_t output_latch(std::uint8_t previous, Output output, bool enabled) {
    const auto mask = bit(static_cast<unsigned>(output));
    return static_cast<std::uint8_t>((enabled ? previous & ~mask : previous | mask) | input_mask) & 0xfe;
}
static_assert(Board::expander_interrupt <= 5, "C3 deep-sleep GPIO must be in RTC domain");
static_assert(output_latch(safe_latch, Output::PeripheralRail, true) == 0xee);
static_assert((safe_latch & input_mask) != 0);
} // namespace plant::bsp::zw_v1
