---
module: <module_id>          # e.g. shader_system
scope: vision                # vision | short-term
last-anchor-commit: 0000000  # short hash of last engine commit this doc was reconciled against
---

# <Module Display Name> — Overview

> One-sentence positioning (≤30 chars).

## Current State

3–5 sentences describing what exists in the engine **today**. Reference real
files via `path:line` anchors when useful. Do **not** describe history.

## Target Design

5–10 bullet points. Each bullet ≤2 lines. State the *what*, not the *how*.

- ...
- ...

## Gap vs Code

| Area | Today | Target |
| --- | --- | --- |
| ... | ... | ... |

## Deep-Dive Index

| File | One-line summary |
| --- | --- |
| `deep-dive/<name>.md` | ... |

## Planned Deep-Dives (TODO)

- [ ] `<name>.md` — short scope
- [ ] ...

## Open Questions

≤5 items. One line each. Resolve by writing an ADR or a deep-dive, then delete from this list.

- ...

## Changelog

≤5 most recent lines. Format: `YYYY-MM-DD <commit-hash>: <summary>`.

- 2026-05-21 0000000: initial skeleton
