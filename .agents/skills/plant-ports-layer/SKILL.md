---
name: plant-ports-layer
description: Design or review platform-neutral capability contracts in 02_ports. Use when adding or changing motion, light, haptic, touch, BLE, OTA, power, storage, clock, logger, or runtime interfaces.
---

# Ports Layer Boundary

`02_ports` states what policy needs from the outside world. A Port is a narrow capability contract, not a mirror of a vendor driver.

## May depend on

- `01_core` only.

## Owns

- Platform-neutral interfaces and bounded transfer types required by Services.
- Return semantics and capability-level contracts that Fake, Linux, and ESP-IDF implementations can share.

## Must not own

- Product behavior or lifecycle policy.
- GPIO numbers, channels, SDK configuration, task handles, callbacks, queues, concrete storage formats, or vendor error codes.
- Adapter construction or BSP selection.

Before adding a method, confirm that policy genuinely needs the capability and that at least plausible alternative implementations can honor the same meaning. Update all implementations and contract tests in the same change.
