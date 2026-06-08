---
module: <module-name>
scope: landed
landed-at-commit: <short-hash-of-completion>
last-anchor-commit: <short-hash-of-last-reconciliation>
# superseded-by: <new-module-name>   # add only when a new roadmap module supersedes this
---

# <Module Name> — Landed Design Rationale

> One sentence (≤30 chars) stating what this module is, in present tense.

## What

Plain description of the **current implementation** as it actually exists in code. 3–5 sentences. Reference real anchors:

- `path/to/file.cpp:LINE` — one-line role
- `path/to/file.h:LINE` — one-line role

Do **not** describe the historical journey. The journey lives in `git log`.

## Why This Design

The single most important section of this document. Record the **reasoning** that produced the current code, structured around concrete alternatives that were considered and rejected.

| Question | Decision | Why not the alternative |
| --- | --- | --- |
| <e.g. Which shader language?> | <e.g. Slang> | <≤80 chars on why HLSL/MSL were rejected> |
| <…> | <…> | <…> |

If a decision needed more than one row to explain, it deserves its own ADR under `decisions/NNNN-*.md`. This table only summarizes; it does not argue.

## Benefits

Concrete, ideally measurable, gains delivered by this design.

- <Benefit 1, with a number if possible — e.g. "shader compile p95: 12s → 2.4s">
- <Benefit 2>
- <Benefit 3>

If a "benefit" is purely aspirational ("cleaner code", "more flexible"), drop it. Rationale documents do not deal in adjectives.

## Trade-offs

What this design **gave up** in exchange for the benefits above. Honesty here is the whole point of the document.

| Cost | Mitigation (if any) |
| --- | --- |
| <e.g. Build-time dependency on Slang toolchain> | <e.g. Vendored under `libs/slang/`, pinned by submodule> |
| <…> | <…> |

## Known Limitations

> Optional section. Append only when a real defect is observed in production use.
> Each entry follows `_template/known-limitation.template.md`. Older entries are
> never deleted — they remain as evidence of why a successor module was needed.

<!-- intentionally empty until a limitation is found -->

## Related Decisions

- [`decisions/0001-<title>.md`](./decisions/0001-<title>.md) — <one-line summary>
- [`decisions/0002-<title>.md`](./decisions/0002-<title>.md) — <one-line summary>

(Omit this section if the module has no ADRs. Do not invent placeholder entries.)

## Changelog

> Append-only. Newest first. Maximum 5 entries — older ones are pruned silently
> (full history is in `git log`). Format: `YYYY-MM-DD <hash>: <≤80 char summary>`.

- YYYY-MM-DD <hash>: landed from `roadmap/<module>/` at this commit.
