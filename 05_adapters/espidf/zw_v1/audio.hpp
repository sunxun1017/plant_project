#pragma once
#include <cstdint>
#include "05_adapters/espidf/zw_v1/hardware.hpp"
namespace plant::zw_v1 {
// Optional bench test. Measures 100ms of mono PCM; not speech recognition.
esp_err_t capture_audio_level(Hardware& hardware, std::uint16_t& level);
esp_err_t prepare_audio_standby(Hardware& hardware);
}
