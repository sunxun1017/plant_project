---
name: plant-docs-layer
description: Maintain authoritative product, architecture, ADR, protocol, development, and board-test documents in 00_docs. Use when behavior, boundaries, wire formats, workflows, assumptions, or acceptance criteria change.
---

# Documentation Layer Boundary

`00_docs` records decisions and contracts that must remain understandable independently of a single implementation diff.

## Ownership

- `product`: user-visible scope, lifecycle, behavior, priorities, safety, OTA, power, and acceptance.
- `architecture`: stable dependency, ownership, and execution-context rules.
- `adr`: lasting decisions with alternatives and consequences.
- `protocol`: public wire formats and compatibility.
- `development`: repository workflows and engineering conventions.
- `testing`: hardware assumptions, procedures, and evidence boundaries.

Do not use documentation to pretend an unimplemented capability exists or an unmeasured target passed. Keep requirements distinct from current implementation status and provisional board values. Update the authoritative document rather than copying divergent rules into a new general README.

Documentation-only changes still require link/path checks, diff review, and the Git workflow. Run code tests when the documentation change accompanies or changes an executable contract.
