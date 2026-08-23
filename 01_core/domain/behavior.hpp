/*
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 18:24:06
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-23 15:05:40
 * @FilePath: /plant_project/01_core/domain/behavior.hpp
 * @Description: behavior
 */
#pragma once

#include <cstdint>

namespace plant {
/**
 * @brief 这是最高层的产品行为
 * 
 */
enum class Behavior : std::uint8_t {
    WakeUp = 0, // 唤醒
    Happy,      // 开心
    Attention,  // TODO： 注意似乎目前产品没有应用场景
    Calm,       // 平静
    Sleep,      // 睡眠
    Error,      // 故障
    Grow,       // 生长
    Retract,    // 收缩
};

// V2 舵机只表达高度变化；具体目标位置由 Growth Service 动态计算。
enum class MotionPattern : std::uint8_t {
    Grow = 0,    // 生长：向更高目标位置移动
    Retract,     // 萎缩：向更低目标位置移动
};

/**
 * @brief 具体外设行为
 * 
 */
enum class LightPattern : std::uint8_t {
    FadeIn = 0,
    SoftBreathing,
    ShortPulse,
    SlowBreathing,
    FadeOut,
    ErrorBlink,
    TouchPulse,
    ListeningBreath,
    SunGlow,
    ComfortGlow,
    GrowthRise,
    GrowthLimit,
};

enum class HapticPattern : std::uint8_t {
    Off = 0,
    SoftPulse,
    DoubleSoftPulse,
    Warning,
};

enum class CompletionTarget : std::uint8_t {
    Idle = 0,
    Sleeping,
    Fault,
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
    LightPattern lighting;
    HapticPattern haptic;
    CompletionTarget completion;
    bool interruptible_by_normal_event;
};

}  // namespace plant
