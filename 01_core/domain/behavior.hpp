/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 16:00:32
 * @FilePath: /plant_project/01_core/domain/behavior.hpp
 * @Description: behavior
 */
#pragma once

#include <cstdint>

#include "01_core/domain/light.hpp"

namespace plant {
/**
 * @brief 这是最高层的产品行为
 * 
 */
enum class Behavior : std::uint8_t {
    WakeUp = 0,  // 唤醒
    Happy = 1,   // 开心
    Sleep = 4,   // 睡眠
    Error = 5,   // 故障
    Grow = 6,    // 生长
    Retract = 7, // 收缩
    None = 0xFF, // 尚未发生上层行为
};

// V2 舵机只表达高度变化；具体目标位置由 Growth Service 动态计算。当下只由它体现生长
enum class MotionPattern : std::uint8_t {
    Grow = 0,    // 生长：向更高目标位置移动
    Retract,     // 萎缩：向更低目标位置移动
};

enum class HapticPattern : std::uint8_t {
    Off = 0,    // 关闭
    SoftPulse,  //
    DoubleSoftPulse,
    Warning,
};

enum class CompletionTarget : std::uint8_t {
    Idle = 0,   // 空闲
    Sleeping,   // 睡眠
    Fault,      // 故障
};

enum class InterruptionReason : std::uint8_t {
    NormalCommand = 0,
    Touch,
    Stop,
    WakeSleep,
    Fault,
};

struct BehaviorPlan {
    Behavior behavior;
    LightCue lighting;
    HapticPattern haptic;
    CompletionTarget completion;
    bool interruptible_by_normal_event;
};

}  // namespace plant
