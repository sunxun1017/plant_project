---
name: plant-composition-layer
description: Compose the Plant V1 firmware product in 07_products and associated ESP-IDF build/config files. Use when wiring dependencies, selecting BSP/adapters/features, scheduling the system loop, booting, partitions, sdkconfig, or product-specific defaults.
---

# Product Composition Boundary

`07_products/plant_v1` is the composition root. Read [plant-config-layer](../plant-config-layer/SKILL.md) as well when changing root ESP-IDF CMake selection, `sdkconfig.defaults`, `partitions.csv`, or `10_config`.

## Owns

- Construction and dependency injection of Services and Adapters.
- BSP/feature selection, initialization order, system-loop integration, and product startup/fault handoff.
- Product build requirements and selection of the applicable configuration profile.

## Must not own

- Reimplemented reusable business rules, protocol parsing, vendor callback logic, or scattered hardware constants.
- Direct business-state mutation that bypasses the owning Service.

Keep the composition root explicit and small. It may know concrete classes, but it should connect them rather than absorb their logic. Runtime polling must stay bounded, preserve the callback/queue boundary, and keep behavior non-blocking.

Any composition/config change requires host integration tests when policy wiring changes and a fresh ESP32-C3 build when sources, components, target Kconfig, partitions, OTA, BLE, or power configuration changes. Check binary headroom and the generated configuration values, not only the exit code.
