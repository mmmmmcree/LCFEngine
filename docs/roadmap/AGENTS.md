# AGENTS.md — Protocol for LLM Collaboration

Any LLM or autonomous agent editing files under `docs/roadmap/` **must** follow this protocol. Humans should follow it too, but it is written primarily for machine readers because that is the harder constraint.

## 0. Read order on entry

1. This file (`AGENTS.md`).
2. `PRINCIPLES.md`.
3. **Exactly one** module's `overview.md` — the one relevant to the current task.
4. *If and only if* the overview is insufficient: at most one `deep-dive/*.md` from the same module.

You are **forbidden** from reading other modules' contents during a single task. Cross-module knowledge belongs in `PRINCIPLES.md`; if it is not there, raise it as an Open Question instead of inferring it.

## 1. Writing rules

### 1.1 Templates are mandatory

- New `overview.md` → copy `_template/overview.template.md` verbatim, then fill.
- New `deep-dive/*.md` → copy `_template/deep-dive.template.md`.
- New ADR → copy `_template/adr.template.md`. ADRs live at `<module>/decisions/NNNN-*.md` (or `_archive/NNNN-*.md` if rejected).

### 1.2 Frontmatter is the only metadata

Only the fields shown in the templates are allowed. Do not invent new keys. The single mandatory commit field is `last-anchor-commit` — the short hash of the engine commit you reconciled the document against. Update it whenever you touch the body.

### 1.3 Size caps are hard limits

| File kind | Max lines |
| --- | --- |
| `overview.md` | 200 |
| `deep-dive/*.md` | 300 |
| ADR | 80 |

If you cannot fit, **split the document**, do not raise the cap.

### 1.4 No code dumps

Pseudocode or interface sketches ≤ 15 lines per block. If a design needs a longer code sample to be understood, the design is not yet ready for this directory — it belongs in source comments or a prototype branch.

## 2. Anti-bloat rules

### 2.1 Delete, do not annotate

When a section conflicts with the current `last-anchor-commit`, **rewrite or delete** it. Do not append "EDIT 2026-..." paragraphs. The git history is the audit trail.

### 2.2 Changelog is bounded

Each document keeps **at most 5** Changelog lines, newest first. Older entries are pruned silently. Format:

```
YYYY-MM-DD <short-hash>: <≤80 char summary>
```

### 2.3 Discussion goes elsewhere

This directory holds *conclusions*, not deliberations. Brainstorming, transcripts, and chat dumps must not appear here. If a rejected idea is likely to be re-proposed, file a single ADR under `_archive/` and move on.

## 3. Decision recording (ADR)

Material direction changes require an ADR before merging. ADRs are short (`Context / Decision / Consequences / Alternatives`), single-page, immutable once accepted. To revisit a decision, write a new ADR with `status: superseded-by-NNNN` on the old one.

## 4. Forbidden actions

- ❌ Adding files outside the structure shown in `README.md`.
- ❌ Reading or editing two modules' contents in a single task.
- ❌ Copying source code into roadmap docs.
- ❌ Adding implementation TODO comments to source files from this directory's text.
- ❌ Creating new top-level files (`*.md` directly under `docs/roadmap/`) without first proposing it via ADR.
- ❌ Mentioning specific LLM/model names or vendor branding inside roadmap content.

## 5. When in doubt

Stop and add an entry to the relevant module's **Open Questions** section. Do not guess. An unresolved question costs nothing; a wrong commitment costs a refactor.
