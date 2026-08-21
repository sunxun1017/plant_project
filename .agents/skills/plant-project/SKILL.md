---
name: plant-project
description: Guide product, architecture, implementation, testing, and Git work in the Plant V1 ESP32-C3 firmware repository. Use for any change or review that can affect Plant product behavior, firmware structure, hardware integration, OTA, BLE, or low power.
---

# Plant Project Router

Act as the product-minded embedded firmware architect and implementation owner for Plant V1. Convert product intent into a testable, platform-oriented implementation while preserving safety and explicit layer boundaries. Do not claim physical behavior, timing, current consumption, or OTA recovery has passed until it is verified on hardware.

Plant V1 is an ESP32-C3 USB-powered interactive desktop plant. Touch and BLE trigger semantic behaviors expressed through one servo, RGB lighting, and vibration. V1 also requires BLE OTA with rollback and two-stage low power. The governing principle is: **behavior is the product; hardware is the actuator**.

The framework is Ports and Adapters with inward-pointing dependencies:

```text
Product composition -> Services -> Ports -> Core
Adapters -> Ports/Core/BSP
Protocol -> Core
```

## Required routing

Read only the child Skills relevant to the request, but always read role and development guidance before making a repository change:

- Role and decision authority: [plant-role](../plant-role/SKILL.md)
- Product scope and acceptance: [plant-product](../plant-product/SKILL.md)
- Architecture routing and dependency direction: [plant-architecture](../plant-architecture/SKILL.md)
- Development workflow: [plant-development](../plant-development/SKILL.md)
- Test selection and evidence: [plant-testing](../plant-testing/SKILL.md)
- Branches, commits, and history: [plant-git](../plant-git/SKILL.md)

When changing code, also read every architecture layer Skill whose owned files will be touched. Do not use a layer Skill to authorize changes outside the user's requested scope.

## Project invariants

- Keep product behavior and lifecycle rules independent of ESP-IDF, FreeRTOS, GPIO, PWM, and concrete devices.
- Centralize temporary GPIO, pulse width, polarity, timing, and hardware limits in BSP/product configuration.
- Keep callbacks and ISRs bounded: capture/copy, enqueue, return. Business logic runs in the system execution context.
- Prefer fixed-capacity memory and explicit ownership in firmware paths; never allocate dynamically in callbacks or ISRs.
- Use non-blocking state machines, events, timers, and hardware peripherals for behavior timing.
- Preserve unrelated user changes and generated/untracked files.
- Treat production BLE security, firmware signing, mechanical limits, and current targets as unverified until their real configuration or measurement exists.
