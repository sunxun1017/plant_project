# ADR 0004: Semantic Light Cues and Adapter-Owned Rendering

## 状态

已接受

## 背景

产品行为、传感能力和硬件表现需要独立演进。此前灯光请求同时携带通用动画模式和
语义效果，调用方需要维护两套可能重复或不一致的匹配关系。

## 决策

灯光边界分为三层：

```text
Product/Service semantic event
        ↓
     LightCue
        ↓
 LightLayer arbitration
        ↓
 Adapter-owned RGB/waveform rendering
```

- `LightCue` 是平台无关的产品语义，例如 `TouchAccepted`、`SunlightExposure`、`Growth`。
- `LightLayer` 只负责所有权、优先级、持续时间和对传感器的遮罩策略。
- `ILightPort` 只接受 `LightCue`；不暴露颜色、PWM、动画模式或周期。
- 具体 Adapter 独占 `LightCue` 到颜色、波形、周期和占空比的匹配。
- 硬件变体可以替换匹配实现，而不修改 Behavior、Growth 或 Sensing Service。

## 运行效率

仲裁保持固定容量数组和最多 7 个 Layer 的线性扫描，不引入动态内存、任务或队列。
LED Adapter 缓存上一次量化后的 RGB 值，连续 Tick 没有产生新占空比时不重复写 LEDC 通道。

## 后果

新增一种产品灯效只需增加语义 Cue 和对应 Adapter 匹配；新增硬件表现只需替换 Adapter
渲染，不需要让业务层知道硬件细节。语义 Cue 的兼容性由 Core/Port 测试保护，具体颜色
和波形仍需在 ESP32-C3 实机上验证。
