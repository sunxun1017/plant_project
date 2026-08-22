#pragma once

#include <cstdint>

#include "01_core/common/status.hpp"
#include "esp_adc/adc_oneshot.h"

namespace plant {

// ESP32-C3 只有一个 ADC1 oneshot unit；V2 的三路模拟输入共享这个所有者。
class V2AdcSampler final {
public:
    Status initialize();
    Status read_raw(adc_channel_t channel, int& raw_value) const;

private:
    adc_oneshot_unit_handle_t unit_{nullptr};
    bool initialized_{false};
};

}  // namespace plant
