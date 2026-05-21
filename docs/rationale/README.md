# LCFEngine Rationale

The single authoritative source for **why the engine is built the way it is** — a permanent record of design intent for *landed* subsystems. This is *not* a record of what the engine is going to become — that lives in [`../roadmap/`](../roadmap/).

## Who reads this

- **Humans** — open whichever module's `overview.md` you need to understand the reasoning behind code you are working on.
- **LLMs / agents** — read [`AGENTS.md`](./AGENTS.md) first, then follow the protocol there.

## How a module gets here

A module appears under this directory only after its roadmap counterpart is **fully implemented**. Migration is mechanical, not editorial — see [`../roadmap/AGENTS.md#module-lifecycle`](../roadmap/AGENTS.md#module-lifecycle) for the exact 5 steps.

```
  roadmap/<module>/   ──── implementation completes ────▶   rationale/<module>/
                              (edge A: git mv + frontmatter rewrite)

  rationale/<module>/ ──── defect discovered ─────────▶   roadmap/<module>_v2/
                              (edge B: append Known Limitation, open new module)
```

## Module Index

> Empty by design. The first module lands here only when its roadmap vision is
> complete. Do not pre-create entries.

| Module | Landed at | Superseded by |
| --- | --- | --- |
| *(none yet)* | — | — |

## Cross-cutting

- [`PRINCIPLES.md`](./PRINCIPLES.md) — principles for **recording landed history** (distinct from roadmap's principles for **exploring the future**).
- [`AGENTS.md`](./AGENTS.md) — protocol any LLM/agent must follow before editing this directory.
- [`_template/`](./_template/) — required skeletons for new module rationales and Known Limitation entries.
- [`_deprecated/`](./_deprecated/) — entire-module shutdowns (rare). A directory only moves here when the corresponding subsystem is removed from the engine.

## Conventions

- **History is immutable.** A landed rationale's `What` / `Why This Design` / `Trade-offs` sections are **never edited** after landing. New information appends to `Known Limitations` instead.
- **One module = one directory**, mirroring its roadmap origin. Cross-module references are forbidden in module bodies; lift shared rules to `PRINCIPLES.md`.
- **Size caps.** A rationale `overview.md` ≤ 200 lines. ADRs ≤ 80 lines. Same as roadmap.
- **No code dumps.** Code/pseudocode in rationale ≤ 15 lines per block.

## Status

This directory is bootstrapped. Skeleton + templates are in place, awaiting the first module to graduate from `../roadmap/`.
