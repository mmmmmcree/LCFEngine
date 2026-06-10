# AGENTS.md — Top-level Routing for `agent-workspace/`

This is the entry point for any LLM or agent touching `agent-workspace/`. It does **not** restate any rules — it routes you to the right sub-protocol. Pick the row that matches your task and read **only that** sub-AGENTS file (plus the corresponding `PRINCIPLES.md`).

Five sibling directories, five different roles:

- `roadmap/` — **the future**: visions and directions, lifespan months to years.
- `rationale/` — **the past**: why landed subsystems are built the way they are, append-only forever.
- `agent-notes/` — **the present**: ephemeral plan documents written by an agent for a specific in-flight feature, deleted when the feature is done.
- `explainers/` — **the bridge**: pedagogical code-reading guides written by an agent on request, helping the human navigate a subsystem they collaborated on building.
- `conventions/` — **the rules**: living style documents iterated through human feedback; read before writing code.

| Task | Sub-protocol to read |
| --- | --- |
| Explore a future direction / write a new vision module | [`roadmap/AGENTS.md`](./roadmap/AGENTS.md) |
| Record why a *landed* subsystem is built the way it is | [`rationale/AGENTS.md`](./rationale/AGENTS.md) |
| Migrate a finished vision into landed history (edge A) | [`roadmap/AGENTS.md#6-module-lifecycle-vision--landed`](./roadmap/AGENTS.md) |
| File a defect against a landed subsystem (edge B) | [`rationale/AGENTS.md#2-edge-b--handling-a-discovered-defect`](./rationale/AGENTS.md) |
| Write or update an ephemeral plan for the current feature | [`agent-notes/AGENTS.md`](./agent-notes/AGENTS.md) |
| Write or update an explainer / source-reading guide | [`explainers/AGENTS.md`](./explainers/AGENTS.md) |
| Write code, or receive style feedback on written code | [`conventions/AGENTS.md`](./conventions/AGENTS.md) |
| Look up shared principles | [`roadmap/PRINCIPLES.md`](./roadmap/PRINCIPLES.md) (future) **or** [`rationale/PRINCIPLES.md`](./rationale/PRINCIPLES.md) (history) — they are deliberately disjoint |

Nothing else lives here yet. Adding a new top-level subdirectory under `agent-workspace/` requires an ADR filed in roadmap's `_archive/` (proposed) or the relevant module's `decisions/`.
