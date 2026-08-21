---
name: plant-testing
description: Select and execute Plant V1 host, sanitizer, protocol, ESP-IDF cross-build, and board verification. Use when implementing, reviewing, validating, or claiming completion of firmware behavior, BLE, OTA, power, or hardware integration.
---

# Plant Verification

Choose evidence according to the changed boundary. Never substitute a lower test level for a required higher one.
When adding or reorganizing repository test code, also read [plant-tests-layer](../plant-tests-layer/SKILL.md).

## Host gates

For Core, Ports, Services, Protocol, or application orchestration changes:

```bash
cmake -S . -B /tmp/plant-project-build -DBUILD_TESTING=ON
cmake --build /tmp/plant-project-build --clean-first --parallel
ctest --test-dir /tmp/plant-project-build --output-on-failure
```

Use ASan/UBSan for parsing, buffers, state-machine refactors, ownership changes, or other memory-sensitive work. Tests should cover success, invalid input, state rejection, interruption, idempotence, and recovery—not only the happy path.

## Target gates

For Adapter, BSP, Product, `sdkconfig.defaults`, partition, BLE, OTA, or power changes, also perform a fresh ESP32-C3 ESP-IDF build using a build directory and sdkconfig under `/tmp`. Verify target, relevant Kconfig values, partition sizes, and firmware headroom.

For the Python BLE utility, run:

```bash
python3 09_tools/protocol_tools/test_plant_ble_tool.py
```

## Board boundary

Read the [board bring-up checklist](../../../00_docs/testing/board_bringup_v1.md) before hardware testing. Host tests and an ESP-IDF build do not verify GPIO routing, mechanical safety, BLE interoperability, sleep current, wake sources, signing, rollback under power loss, or 24-hour stability. Report those items as pending until measured on the ESP32-C3 board.
