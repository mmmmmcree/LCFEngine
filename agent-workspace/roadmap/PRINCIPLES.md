# PRINCIPLES.md — Cross-cutting Design Principles

These are the rules every module in `agent-workspace/roadmap/` shares. A module document **must not** restate them; it may only reference them by section number (e.g. `see PRINCIPLES §3.2`).

If a principle here is in tension with a module's design, the module wins **only after** filing an ADR that makes the deviation explicit.

## 1. Vision vs reality

1.1 This directory describes the **target state** of the engine, not its current state.
1.2 Current state lives in code, code comments, and `CLAUDE.md`. Do not duplicate it here.
1.3 A roadmap document is "alive" only while its `last-anchor-commit` is within ~30 commits of `HEAD`. Older docs are presumed stale and must be reviewed before being trusted.

## 2. LLM-friendliness as a first-class constraint

2.1 Every file must be intelligible **in isolation**. Assume the reader has only this file plus `PRINCIPLES.md` in context.
2.2 Prefer **tables and short bullets** over prose. Long paragraphs cost tokens and rarely add precision.
2.3 No diagrams unless they are pure-text (ASCII / mermaid). Image assets are forbidden in this directory.
2.4 No links to external sites that may rot. Cite books/papers by title + author + year, not URL.

## 3. Modularity

3.1 A module is a **closed world**. Its docs reference its own files plus this `PRINCIPLES.md` and nothing else under `agent-workspace/roadmap/`.
3.2 If two modules need to agree on something, that something is promoted into `PRINCIPLES.md`. There is no other shared surface.
3.3 A module that grows beyond 6 deep-dives is a sign it should be split. Splitting is cheap; entanglement is not.

## 4. Decisions over discussions

4.1 Roadmap docs record **conclusions**. Reasoning lives in ADRs, not in narrative text.
4.2 Every "we picked X over Y because…" must either fit in a `Trade-offs` table or become an ADR.
4.3 Once an ADR is accepted, the body of the affected docs is rewritten to assume the decision; the ADR is the only place the alternatives survive.

## 5. Anti-bloat

5.1 The default action on stale content is **delete**, not annotate.
5.2 Frontmatter has exactly the fields shown in the templates. Adding a field requires an ADR.
5.3 Changelogs are capped at 5 entries. Older history lives in `git log`.

## 6. Engine-wide invariants every module must respect

These are non-negotiable assumptions shared by all subsystems. Modules that need to violate one must file an ADR.

| # | Invariant |
| --- | --- |
| 6.1 | Editor-only code never ships in runtime binaries; runtime never depends on editor headers. |
| 6.2 | GPU resources are owned by RHI; no subsystem holds raw `Vk*` handles outside RHI. |
| 6.3 | Frame-coupled data is multi-buffered; no implicit serialization through single-copy resources. |
| 6.4 | All cross-thread communication goes through the task system or explicit lock-free queues — never bare `std::mutex` on hot paths. |
| 6.5 | Asset identity is stable under content edits; runtime paths are content-hash-addressed, not name-addressed. |
| 6.6 | No subsystem may introduce its own logging, allocator, or job pool — use the engine-wide ones. |
| 6.7 | `<api>_core` SDK substrate layers (e.g. `libs/vk_core`) are engine-policy-free: non-blocking public surface, leaf classes hold handles not context pointers, no dependency on `gui/` or engine modules, no frame-loop concepts. |
| 6.8 | Vulkan baseline is 1.2 Timeline Semaphores; new render-stack code has no `VkFence` path. |

## 7. Glossary

Only terms that recur across modules. Module-local jargon stays in the module.

| Term | Meaning |
| --- | --- |
| **RHI** | Render Hardware Interface — the engine's API-agnostic GPU abstraction. |
| **DDC** | Derived Data Cache — content addressed cache of artefacts derived from source assets. |
| **PSO** | Pipeline State Object — the GPU-side immutable pipeline configuration. |
| **Bindless** | Resource access model where shaders index into global descriptor arrays rather than binding by slot. |
| **Variant** | A specialized compilation of a shader for a specific combination of feature flags. |
| **System (ECS)** | A function operating on entities matching a given component query, scheduled by the task system. |
