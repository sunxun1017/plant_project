---
name: plant-protocol-layer
description: Maintain transport-independent Plant wire messages and codecs in 04_protocol. Use when changing BLE command frames, response fields, OTA chunk encoding, CRC, protocol versions, validation, or message compatibility.
---

# Protocol Layer Boundary

`04_protocol` owns byte-level message definition, encoding, decoding, version handling, length/bounds checks, and integrity checks. It does not own BLE or execute a command.

## May depend on

- Platform-neutral `01_core` domain types.

## Must not depend on

- BLE, Wi-Fi, UART, USB, sockets, ESP-IDF, FreeRTOS, Adapters, BSP, or Product composition.

Maintain deterministic fixed-capacity frames and explicit little-endian fields. Validate magic, version, flags, exact length, enum ranges, payload bounds, and CRC before execution. Struct memory layout is never the wire format.

When changing public bytes, update the protocol version or compatibility logic as appropriate, the protocol document under `00_docs/protocol`, C++ codec tests, and the Python tool/test vectors. Test maximum OTA chunks, truncation, excess length, unknown commands, invalid enums, corrupt CRC, and request/response correlation.
