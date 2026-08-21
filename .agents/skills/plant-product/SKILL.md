---
name: plant-product
description: Preserve Plant V1 product scope, behaviors, lifecycle priorities, BLE OTA, and low-power acceptance criteria. Use when a change affects user interaction, commands, state, safety, OTA, power, configuration, or telemetry.
---

# Plant V1 Product Boundary

Before changing product semantics, read the authoritative [product requirements](../../../00_docs/product/product_requirements_v1.md). For behavior triggers, channel patterns, interruption, or completion, also read the [behavior mapping](../../../00_docs/product/behavior_mapping_v1.md).

## V1 product

Plant V1 is a USB-powered ESP32-C3 desktop plant with one servo, RGB lighting, vibration, touch input, and BLE. It must support `Booting`, `Idle`, `Interacting`, `Sleeping`, `Fault`, and `Updating`, plus the semantic behaviors `WakeUp`, `Happy`, `Attention`, `Calm`, `Sleep`, and `Error`.

Preserve these non-negotiable outcomes:

- Touch and BLE produce coherent semantic behavior, not direct actuator commands.
- Behavior sequences are non-blocking and remain interruptible according to event priority.
- Fault and stop paths make motion and vibration safe.
- Sleep reaches a safe mechanical pose before outputs are disabled.
- Light sleep preserves the intended touch/BLE wake capability; deep sleep closes BLE and is gated by connection, OTA, and flash activity.
- BLE OTA validates product/hardware/version/length, uses the system command path, activates only a complete image, confirms the new image after self-test, and can roll back.

V1 excludes Wi-Fi/cloud, native mobile apps, batteries, environment sensors, sound, and additional motion axes. Do not let these future capabilities block V1 or leak speculative abstractions into a focused change.

If implementation and the product documents disagree, identify the mismatch explicitly. Change the requirement only when the user is changing product intent; otherwise change the implementation and tests.
