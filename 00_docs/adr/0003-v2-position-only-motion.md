# ADR 0003: V2 使用位置闭环运动

## 状态

已接受

## 背景

Plant V2 的舵机只表达植物高度：生长时提高目标位置，萎缩时降低目标位置。
V1 遗留的唤醒、摆动、抬头和固定睡眠姿态等表现性运动模式不属于 V2 产品行为。

## 决策

V2 只使用包含 `Grow` 和 `Retract` 的 `MotionPattern`，不再由 `BehaviorService` 驱动表现性舵机动作。

- `BehaviorService` 的普通行为只负责灯光和振动，并在行为切换或故障时请求运动安全停止。
- `Behavior::Grow/Retract` 映射为 `MotionPattern::Grow/Retract`，并通过
  `IPositionMotionPort::move_to()` 作为生长和萎缩的唯一运动启动入口。
- `IPositionMotionPort::poll()` 负责位置闭环运动的完成和故障反馈。
- 绝对目标位置、方向、机械限位和反馈校验继续由 Growth Service、BSP 和位置 Adapter 负责。

## 后果

V2 的舵机不会因为 Happy、WakeUp 或 Sleep 播放额外姿态动作，避免产品语义和机械运动边界混淆。
V1 的表现性运动源文件保留为历史代码，但不再被 V2 构建入口或 V2 测试编译。
