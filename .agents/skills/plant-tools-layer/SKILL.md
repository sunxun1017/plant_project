---
name: plant-tools-layer
description: Build and review developer-facing utilities in 09_tools for protocol diagnostics, flashing, generation, and board validation. Use when adding scripts or command-line helpers that interact with firmware artifacts or devices.
---

# Tools Layer Boundary

`09_tools` supports development and validation; it is not part of the firmware dependency graph.

Tools may encode public protocol contracts or automate documented workflows, but must not become the only definition of those contracts. Keep protocol constants synchronized with `04_protocol` and `00_docs/protocol`, and add deterministic host tests for codecs and transformations.

Import optional hardware dependencies only for commands that need them, so pure codec/tests remain runnable without device packages. Validate paths, bounds, versions, and destructive targets. Do not embed credentials, production signing keys, machine-specific absolute paths, or hidden device mutations.

Clearly distinguish generation/inspection from actions such as flashing, provisioning, erasing, signing, or connecting to a board; the latter require the user's corresponding authorization and an explicit target.
