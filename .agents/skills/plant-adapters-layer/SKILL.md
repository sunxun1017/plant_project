---
name: plant-adapters-layer
description: Implement or review platform and hardware boundaries in 05_adapters. Use for ESP-IDF, Linux, or Fake implementations; GPIO/peripheral drivers; BLE callbacks; OTA flash; power management; storage; and vendor error conversion.
---

# Adapters Layer Boundary

`05_adapters` implements Ports using a vendor SDK, operating system, or test environment. Platform details terminate here.

## May depend on

- `01_core`, `02_ports`, applicable `06_bsp`, vendor SDK, and operating-system APIs.

## Owns

- Vendor handle/resource lifecycle, concrete peripheral operations, callback/ISR translation, bounded queues, and vendor-error conversion.
- Enforcement of electrical and mechanical limits supplied by BSP/configuration.

## Must not own

- Product behavior selection, event priority, lifecycle transitions, OTA business eligibility, or wire-protocol meaning.
- Constants that belong to a board/product configuration.
- Vendor types in a Port or Service public API.

Callbacks and ISRs only capture/copy, enqueue/signal, and return; do not allocate dynamically or control behavior there. Keep long operations out of the system task or expose them as an explicit asynchronous boundary. For ESP-IDF changes, require an ESP32-C3 cross-build and preserve board-validation handoff.
