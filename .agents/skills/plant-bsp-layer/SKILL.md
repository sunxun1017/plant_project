---
name: plant-bsp-layer
description: Centralize and review Plant board identity, GPIO mapping, peripheral presence, electrical polarity, timing, pulse, and safe hardware limits in 06_bsp. Use whenever provisional or confirmed board values change.
---

# BSP Layer Boundary

`06_bsp` answers which board is present and how its hardware is connected. It is the single owner for provisional ESP32-C3 GPIO, PWM frequency, pulse widths, polarity, peripheral limits, and board revision values.

## May contain

- Pin/peripheral mappings, board capabilities, hardware revisions, electrical polarity, calibrated ranges, and compile-time consistency checks.

## Must not contain

- Product behavior mappings, BLE command semantics, lifecycle/OTA policy, communication policy, state machines, or vendor driver calls.

When real hardware data is missing, use one coherent provisional set and label it for board validation. Never duplicate a guessed GPIO, pulse, or duty constant in Services, Adapters, or Product code. Add static assertions for internal ranges and update the bring-up checklist when an assumption changes.

BSP compilation proves only type/range consistency. Real pin routing, pulse safety, actuator travel, polarity, and current must be verified on the board before removing the provisional status.
