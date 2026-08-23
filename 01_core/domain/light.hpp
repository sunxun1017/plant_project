/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-23 15:52:55
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 15:59:48
 * @FilePath: /plant_project/01_core/domain/light.hpp
 * @Description: 
 */
#pragma once

#include <cstdint>

namespace plant {

// 灯光只表达产品语义；颜色、波形、周期和占空比由具体 Light Adapter 匹配。
// 灯光可能是一种玩法 因此用了匹配机制
enum class LightCue : std::uint8_t {
    Default = 0,
    WakeUp, // 唤醒
    Happy,  // 开心
    Sleep,
    Error,
    TouchAccepted,
    Listening,
    SunlightExposure,
    Comfort,
    Growth,
    GrowthLimit,
};

}  // namespace plant
