---
name: plant-services-layer
description: Implement or review Plant business state owners and workflows in 03_services. Use when changing behavior, lifecycle, communication, OTA, power, diagnostics, motion, lighting, or haptic policy.
---

# Services Layer Boundary

`03_services` owns cohesive product policy and state transitions. Services coordinate semantic capabilities through Ports and return platform-neutral status.

## May depend on

- `01_core`
- `02_ports`
- `04_protocol` when protocol translation is genuinely part of the service boundary

## Must not depend on

- ESP-IDF, FreeRTOS, BSP, GPIO, PWM, LEDC, RMT, MCPWM, NVS, or concrete Adapter classes.

## Ownership rules

- Lifecycle state has one owner; behavior state has one owner; OTA and power flows expose snapshots rather than shared mutable fields.
- Services use semantic patterns and capabilities, never board parameters.
- Public operations are bounded and non-blocking. Long hardware work is started through a Port and completed through events/poll results.
- Use direct calls for synchronous single-target work; introduce queues/events only at true asynchronous boundaries.

Test state tables, priority/interruption, idempotence, invalid-state rejection, partial failure, cancellation, timeout, and recovery with Fake Ports. A Service test must not require ESP-IDF.
