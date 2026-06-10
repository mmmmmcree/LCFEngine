# AGENTS.md — Protocol for LLM Collaboration on Landed Rationale

Any LLM or autonomous agent editing files under `agent-workspace/rationale/` **must** follow this protocol. The rules differ from `../roadmap/AGENTS.md` in one critical way: **rationale is history, not work-in-progress**. Most edits you might want to make are forbidden here.

## 0. Read order on entry

1. This file (`AGENTS.md`).
2. `PRINCIPLES.md` (this directory's, **not** roadmap's).
3. **Exactly one** module's `overview.md` — the one relevant to the current task.
4. *If and only if* the overview is insufficient: at most one ADR from the same module's `decisions/`.

Cross-module reading is forbidden during a single task — the same isolation rule as roadmap.

## 1. The append-only law

This is the most important rule in this directory. **Internalize it before editing anything.**

> Once a module lands here, the sections **What**, **Why This Design**, **Benefits**, and **Trade-offs** are immutable. They are historical record, not living documentation.

You are allowed to:

- ✅ Append a new entry to `## Known Limitations`.
- ✅ Append a new ADR under `<module>/decisions/` describing a follow-up decision.
- ✅ Update the `last-anchor-commit` frontmatter field after re-reading the code and confirming the doc still matches.
- ✅ Add `superseded-by: <new-module>` to frontmatter when a successor module ships.
- ✅ Append (not rewrite) Changelog lines, capped at 5.

You are **forbidden** from:

- ❌ Rewriting any sentence in `What` / `Why This Design` / `Benefits` / `Trade-offs`.
- ❌ Reordering or deleting entries in `## Known Limitations`.
- ❌ "Cleaning up" or "modernizing" the writing of a landed module.
- ❌ Removing a module from `_deprecated/` once it has been moved there.

If you believe a landed rationale is **factually wrong about its own intent**, that is the only legitimate reason to edit a frozen section, and it requires a new ADR explaining what was wrong and why correction (not annotation) is appropriate. This should happen approximately never.

## 2. Edge B — handling a discovered defect

When real-world use of a landed subsystem reveals a defect, the workflow is:

1. Confirm the defect is reproducible and not a misconfiguration.
2. Append a new entry to the module's `## Known Limitations` section, using `_template/known-limitation.template.md` verbatim.
3. If severity is `high` or `blocker`, **also** create a new roadmap module (typically `<module>_v2/` or a more precise name) under `../roadmap/`. Do **not** edit the old rationale's design sections.
4. The new roadmap module's `Current State` references the old rationale; the old rationale's frontmatter eventually gains `superseded-by: <new-module>` once v2 lands.

The old rationale **stays in place forever**. It is the record of why the code was once shaped that way, and that record retains value even after the code is replaced.

## 3. Frontmatter rules

Only these fields are allowed:

```
---
module: <name>                 # required, matches directory name
scope: landed                  # required, fixed value
landed-at-commit: <hash>       # required, the commit at which migration from roadmap completed
last-anchor-commit: <hash>     # required, last time anyone reconciled this doc with HEAD
superseded-by: <module>        # optional, present iff a v2 module exists
---
```

No other keys. Adding one requires an ADR.

## 4. Templates are mandatory

- New module rationale (after edge A migration) → start from `_template/module.template.md`.
- New Known Limitation entry → start from `_template/known-limitation.template.md`.
- New ADR → start from `../roadmap/_template/adr.template.md` (shared with roadmap; the template itself is the same artefact regardless of which side files it).

## 5. Size caps are hard limits

| File kind | Max lines |
| --- | --- |
| `<module>/overview.md` | 200 |
| ADR | 80 |
| Single Known Limitation entry | 30 |

Total Known Limitation entries per module ≤ 8. Beyond that, the module has failed and a `_v2` roadmap is overdue.

## 6. Forbidden actions

- ❌ Pre-creating a `<module>/` directory before its roadmap counterpart has actually landed.
- ❌ Copying source code into rationale docs.
- ❌ Mentioning specific LLM/model names or vendor branding inside rationale content.
- ❌ Adding files outside the structure shown in `README.md`.
- ❌ Importing roadmap-style sections (`Open Questions`, `Planned Deep-Dives`, `Gap vs Code`) — those belong only in roadmap.

## 7. When in doubt

If editing seems necessary but you cannot fit it into "append a Known Limitation" or "append an ADR", **stop**. Either the change belongs in roadmap as a new vision, or it does not belong in writing at all.

## See Also

- [`PRINCIPLES.md`](./PRINCIPLES.md) — principles specific to this directory.
- [`../roadmap/AGENTS.md`](../roadmap/AGENTS.md) — sibling protocol for the future-facing side.
- [`../roadmap/AGENTS.md#module-lifecycle`](../roadmap/AGENTS.md#module-lifecycle) — edge A migration steps (vision → landed).
