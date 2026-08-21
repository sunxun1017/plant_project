---
name: plant-architecture
description: Route Plant firmware changes through Ports and Adapters layers and enforce high-cohesion, low-coupling, platform-replaceable dependencies. Use for design, refactoring, dependency review, new modules, or changes spanning more than one source layer.
---

# Plant Architecture Router

Read the [architecture overview](../../../00_docs/architecture/overview.md) and [dependency rules](../../../00_docs/architecture/dependency_rules.md) when changing boundaries or adding a module. Read the [threading model](../../../00_docs/architecture/threading_model.md) for callbacks, tasks, queues, timers, shared state, or blocking work.

The architecture goal is platform-independent product policy with cohesive state owners and replaceable implementation details. Prefer the smallest interface that represents a stable capability. Do not add abstraction merely because a concrete class exists.

## Layer routing

Read each applicable child Skill before editing its files:

- `00_docs`: [plant-docs-layer](../plant-docs-layer/SKILL.md)
- `01_core`: [plant-core-layer](../plant-core-layer/SKILL.md)
- `02_ports`: [plant-ports-layer](../plant-ports-layer/SKILL.md)
- `03_services`: [plant-services-layer](../plant-services-layer/SKILL.md)
- `04_protocol`: [plant-protocol-layer](../plant-protocol-layer/SKILL.md)
- `05_adapters`: [plant-adapters-layer](../plant-adapters-layer/SKILL.md)
- `06_bsp`: [plant-bsp-layer](../plant-bsp-layer/SKILL.md)
- `07_products`: [plant-composition-layer](../plant-composition-layer/SKILL.md)
- `08_tests`: [plant-tests-layer](../plant-tests-layer/SKILL.md)
- `09_tools`: [plant-tools-layer](../plant-tools-layer/SKILL.md)
- `10_config` and build defaults: [plant-config-layer](../plant-config-layer/SKILL.md)

## Cross-layer rules

- Each mutable business state has one owner; collaborators use commands, events, snapshots, and public interfaces.
- Use direct calls for bounded synchronous work with one target and a return value. Use events/queues for genuinely asynchronous, cross-context, or broadcast work.
- A Service does not own a task by default. Threads are execution resources, not module boundaries.
- Vendor callbacks and ISRs never call product behavior directly.
- Vendor types and errors are translated inside Adapters.
- Important dependency or ownership changes require a focused architecture commit and an ADR under `00_docs/adr` when the decision has lasting alternatives or consequences.
