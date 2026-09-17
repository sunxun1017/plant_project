#include "06_bsp/zw_v1/board.hpp"
#include "05_adapters/espidf/zw_v1/sensor_codec.hpp"
using namespace plant::bsp::zw_v1;
using namespace plant::zw_v1;
constexpr bool check_output_isolation() {
    for (unsigned latch = 0; latch < 256; ++latch) {
        for (auto pin : {1, 2, 3, 4, 6, 7}) {
            for (auto enabled : {false, true}) {
                const auto value = output_latch(latch, static_cast<Output>(pin), enabled);
                if (!(value & bit(5)) || (value & bit(0))) return false;
                if (((value & bit(pin)) == 0) != enabled) return false;
                const auto others = static_cast<std::uint8_t>(~(bit(pin) | bit(5) | bit(0)));
                if ((value & others) != (latch & others)) return false;
            }
        }
    }
    return true;
}
constexpr std::array<std::uint8_t, 6> frame(std::uint16_t t, std::uint16_t h) {
    return {static_cast<std::uint8_t>(t >> 8), static_cast<std::uint8_t>(t), crc8(t >> 8, t),
            static_cast<std::uint8_t>(h >> 8), static_cast<std::uint8_t>(h), crc8(h >> 8, h)};
}
constexpr bool reject_corruption() {
    const auto good = frame(0x6666, 0x8000);
    for (unsigned byte = 0; byte < good.size(); ++byte) {
        for (unsigned b = 0; b < 8; ++b) {
            auto bad = good;
            bad[byte] ^= bit(b);
            if (decode_climate(bad).valid) return false;
        }
    }
    return true;
}
static_assert(check_output_isolation());
static_assert(standby_latch == 0xae);
static_assert((standby_latch & (bit(1) | bit(2) | bit(3) | bit(7))) ==
              (bit(1) | bit(2) | bit(3) | bit(7)), "Standby must disable every actuator and charging");
static_assert(touch_pressed(0xde) && !touch_pressed(0xfe));
static_assert(decode_climate(frame(0, 0)).temperature_centi_c == -4500);
static_assert(decode_climate(frame(65535, 65535)).temperature_centi_c == 13000);
static_assert(decode_climate(frame(0, 0)).humidity_tenths_percent == 0);
static_assert(decode_climate(frame(65535, 65535)).humidity_tenths_percent == 1000);
static_assert(decode_climate(frame(0x6666, 0x8000)).temperature_centi_c == 2500);
static_assert(decode_climate(frame(0x6666, 0x8000)).humidity_tenths_percent == 565);
static_assert(reject_corruption());
// Compile-only tests: all assertions are evaluated by the C++17 compiler.
