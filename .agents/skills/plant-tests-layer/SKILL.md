---
name: plant-tests-layer
description: Design and organize Plant test code in 08_tests across unit, integration, contract, HIL, and Fake-based boundaries. Use when adding tests, moving test ownership, creating fakes, or reviewing whether behavior is verified at the correct level.
---

# Tests Layer Boundary

`08_tests` verifies observable contracts without leaking test-only behavior into production code.

## Test ownership

- `unit`: one domain/service/codec policy with controlled collaborators.
- `integration`: multiple real policy components wired through Fake Ports.
- `contract`: every Adapter implementation obeys the same Port meaning.
- `hil`: assertions requiring a real board, peripheral, BLE peer, power measurement, reset, or flash-failure setup.

Fakes implement Ports and expose observations; they do not duplicate the production state machine or return success so broadly that failures become invisible. Prefer semantic assertions over private implementation coupling.

Every bug fix should include a failing-at-baseline regression test at the lowest level that reproduces the real fault. Keep hardware-only claims in HIL/bring-up evidence, never in a host unit-test name.
