# AGENTS.md — Top-level Routing for `docs/`

This is the entry point for any LLM or agent touching `docs/`. It does **not** restate any rules — it routes you to the right sub-protocol. Pick the row that matches your task and read **only that** sub-AGENTS file (plus the corresponding `PRINCIPLES.md`).

| Task | Sub-protocol to read |
| --- | --- |
| Explore a future direction / write a new vision module | [`roadmap/AGENTS.md`](./roadmap/AGENTS.md) |
| Record why a *landed* subsystem is built the way it is | [`rationale/AGENTS.md`](./rationale/AGENTS.md) |
| Migrate a finished vision into landed history (edge A) | [`roadmap/AGENTS.md#6-module-lifecycle-vision--landed`](./roadmap/AGENTS.md) |
| File a defect against a landed subsystem (edge B) | [`rationale/AGENTS.md#2-edge-b--handling-a-discovered-defect`](./rationale/AGENTS.md) |
| Look up shared principles | [`roadmap/PRINCIPLES.md`](./roadmap/PRINCIPLES.md) (future) **or** [`rationale/PRINCIPLES.md`](./rationale/PRINCIPLES.md) (history) — they are deliberately disjoint |

`roadmap/` is the future, `rationale/` is the past. Nothing else lives here yet. Adding a new top-level subdirectory under `docs/` requires an ADR filed in roadmap's `_archive/` (proposed) or the relevant module's `decisions/`.
