---
name: plant-development
description: Run the repository development workflow from discovery through implementation, verification, documentation, and handoff. Use for any requested code or documentation change in Plant V1.
---

# Plant Development Workflow

1. Inspect `git status`, the current branch, relevant code, and authoritative documents before editing. Preserve unrelated changes.
2. Identify the product outcome, state owner, affected architecture layers, hardware assumptions, and test evidence required.
3. Read [plant-git](../plant-git/SKILL.md) and create or reuse an appropriately scoped branch before the first task edit.
4. Implement the smallest complete vertical slice. Work from stable policy outward: Core/Ports, Services/Protocol, Adapters/BSP, then Product composition.
5. Keep provisional board values in `06_bsp/plant_v1/plant_v1_board.hpp` or the applicable centralized product configuration.
6. Add or update tests with the behavior. Update protocol/product/bring-up docs when public behavior, wire format, hardware assumptions, OTA, or power changes.
7. Read [plant-testing](../plant-testing/SKILL.md), run proportionate verification, inspect `git diff --check`, and review the complete diff.
8. Create logical commits following [plant-git](../plant-git/SKILL.md). Report verified results and explicitly hand off remaining board checks.

## Completion standard

A change is not complete merely because it compiles. It must preserve layer boundaries, handle failure/state transitions, include relevant tests, build for the affected target, and leave no undocumented mismatch with the product requirement.

Do not flash hardware, push branches, rewrite shared history, provision keys, or change production security state unless the user separately authorizes that action.
