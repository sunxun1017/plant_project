---
name: plant-core-layer
description: Define and review platform-neutral Plant domain types and invariants in 01_core. Use when changing commands, events, behavior/state models, status/errors, fixed buffers, configuration value types, or domain snapshots.
---

# Core Layer Boundary

`01_core` is the most stable policy-independent vocabulary. It owns domain values and invariants that can compile on a normal host without ESP-IDF.

## May contain

- Behavior, lifecycle, command, event, status, error, metadata, snapshot, and bounded utility types.
- Platform-neutral validation intrinsic to a value.
- Standard-library types appropriate for embedded use, favoring explicit fixed capacity in firmware data paths.

## May depend on

- Other `01_core` headers.
- The C++ standard library.

## Must not contain or depend on

- Ports, Services, Protocol codecs, Adapters, BSP, Product composition, ESP-IDF, FreeRTOS, POSIX, GPIO, or concrete devices.
- Orchestration, task ownership, transport behavior, persistence, vendor handles, or board constants.
- Mutable global business state.

Keep types semantic: `Behavior::Happy`, not servo degrees; `ErrorCode::Timeout`, not `ESP_ERR_TIMEOUT`. Add host tests for bounds, defaults, invalid values, and compatibility-sensitive enum/wire changes.
