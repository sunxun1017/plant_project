/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 12:24:29
 * @FilePath: /plant_project/05_adapters/espidf/sensing/v2_adc_sampler.hpp
 * @Description: adc's adapter
 */
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
