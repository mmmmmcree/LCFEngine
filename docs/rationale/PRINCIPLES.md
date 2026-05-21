# PRINCIPLES.md — Principles for Recording Landed Rationale

These principles govern `docs/rationale/`. They are deliberately **disjoint** from `../roadmap/PRINCIPLES.md`: roadmap principles describe how to *explore the future*; these describe how to *preserve the past*. A module document **must not** restate these — it may only reference them by section number (e.g. `see PRINCIPLES §2.1`).

## 1. History is immutable

1.1 A landed rationale's design narrative (`What`, `Why This Design`, `Benefits`, `Trade-offs`) is **frozen at landing**. It is the answer to "why did we do it that way at the time", and that answer cannot change retroactively.
1.2 New information after landing has exactly two homes: a new `Known Limitations` entry, or a new ADR. Never an edit to the original prose.
1.3 If hindsight reveals the original rationale was *factually wrong about its own intent* (rare), correction requires a dedicated ADR and is the only exception to 1.1.

## 2. Why beats what

2.1 The point of a rationale document is **the reasoning that produced the code**, not a description of the code. Code already describes itself; comments describe local intent; rationale describes engine-wide intent.
2.2 Every `Why This Design` row pairs a decision with **a concrete alternative that was rejected**. Decisions without rejected alternatives are not decisions, they are defaults — and defaults do not need rationale entries.
2.3 Aspirational benefits ("cleaner", "more flexible", "future-proof") are forbidden. If a benefit cannot be stated as an observable fact (a number, a removed dependency, a deleted line of code), it is not a benefit, it is marketing.

## 3. Anchors over abstractions

3.1 The `What` section cites **real files at real line numbers**. Abstract architecture descriptions belong in roadmap; rationale is anchored in code.
3.2 If an anchor goes stale (the line moved or the file was renamed), update the anchor and bump `last-anchor-commit`. Never replace a precise anchor with a vague one.
3.3 ASCII / mermaid diagrams are permitted; image assets are not. Rationale must remain text-greppable forever.

## 4. Limitations must be falsifiable

4.1 A `Known Limitations` entry that cannot be reproduced is rumour, not knowledge — keep it out.
4.2 Each entry states a minimal trigger and an observable symptom. If you cannot state both, investigate first, file later.
4.3 Severity `blocker` is reserved for "the engine cannot ship until this is fixed" — using it loosely devalues the signal.

## 5. Modularity (shared with roadmap)

5.1 A rationale module is a closed world. It references its own files, this `PRINCIPLES.md`, and nothing else under `docs/rationale/`.
5.2 Cross-module facts are promoted to `../roadmap/PRINCIPLES.md §6` (engine-wide invariants). There is no separate cross-cutting surface on the rationale side — invariants are invariants regardless of which directory observes them.

## 6. Anti-bloat

6.1 Rationale Changelogs are capped at 5 entries, same as roadmap.
6.2 Frontmatter has exactly the fields shown in `AGENTS.md §3`. Adding a field requires an ADR.
6.3 More than 8 Known Limitation entries on a single module is a signal that the module has structurally failed — open a `_v2` roadmap rather than continuing to file limitations.

## See Also

- [`../roadmap/PRINCIPLES.md`](../roadmap/PRINCIPLES.md) — sibling principles, **disjoint content**, governing the future-facing side. Engine-wide invariants (section 6) are defined there and apply to both directories.
- [`AGENTS.md`](./AGENTS.md) — operational rules implementing the principles above.
