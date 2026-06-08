---
parent: <module_id>          # must match an existing module directory
title: <kebab-case-title>    # matches filename without .md
last-anchor-commit: 0000000
---

# <Title>

## Problem

Why this deep-dive exists. What pain in the current engine motivates it.
3–6 sentences. Cite `path:line` when relevant.

## Design

Bulleted requirements + a minimal sketch. Code/pseudocode blocks ≤15 lines.
If a sketch needs more, the content belongs in source comments instead.

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| ... | ... | ... | ✅ chosen / ❌ rejected |

Rejected options that are likely to be re-proposed should also get an ADR
filed under `_archive/` so future LLMs do not re-litigate them.

## Landing Plan

Phased checklist. Each phase ≤5 items, each item ≤1 line.

- [ ] Phase 1 — ...
- [ ] Phase 2 — ...

## Changelog

- 2026-05-21 0000000: created
