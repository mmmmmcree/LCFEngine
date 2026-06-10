# AGENTS.md — Protocol for Living Convention Documents

This directory holds **convention documents** — coding style and similar prescriptive rules — maintained by the agent and iterated through human feedback. They are present-tense and durable: not vision (`roadmap/`), not history (`rationale/`), not ephemeral (`agent-notes/`), not pedagogy (`explainers/`).

## 0. The feedback loop (the reason this directory exists)

```
agent writes code following the current rules
   │
   ▼
human reviews, gives style feedback in chat
   │
   ▼
agent updates the relevant convention doc IN THE SAME TASK
   (add / sharpen / delete rules — never append a correction log)
   │
   ▼
next code-writing task starts from the improved rules
```

A style correction that lands only in chat is a bug in this protocol.

## 1. Reading rules

- **Before writing or reviewing code**, read the convention doc for that language/domain (currently [`cpp-style.md`](./cpp-style.md)).
- Precedence for style specifics: **conventions doc > CLAUDE.md > inferred codebase idiom**. The conventions doc records the human's latest confirmed corrections; if it contradicts CLAUDE.md, the doc is newer — flag the contradiction to the human so CLAUDE.md can be fixed, but follow the doc meanwhile.

## 2. Writing rules

- One file per language/domain (`cpp-style.md`, future: `cmake-style.md`, `shader-style.md`). No per-module style files.
- Rules are **atomic and imperative**, grouped by topic. Each rule may carry a one-line *why* when the reason is not obvious; include a minimal good/bad example only when words are ambiguous.
- **Iterate in place**: superseded rules are rewritten or deleted, never annotated with "EDIT:" trails. Git is the history.
- Record only rules the human has stated or confirmed. Do not invent rules from general taste; if the codebase is inconsistent and the human has not ruled, list it under **Unsettled** and ask when it next matters.
- No size cap, but dedupe aggressively — an unreadable style doc defeats itself.

## 3. Forbidden

- ❌ Restating rules verbatim from CLAUDE.md without refinement (link or refine, do not copy).
- ❌ Chat transcripts or feedback quotes — distill into rules.
- ❌ Design decisions (layering, ownership, API shape) — those belong in `roadmap/` modules; this directory is about *how code reads*, not *what code does*.

## See Also

- [`../AGENTS.md`](../AGENTS.md) — top-level routing.
- ADR-0001 (`../roadmap/_archive/0001-add-conventions-directory.md`) — why this directory exists.
