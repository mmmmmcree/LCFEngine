# AGENTS.md — Protocol for Ephemeral Agent Plan Notes

This directory holds **plan documents written by an agent** while collaborating with the human on a specific feature. Plans here are **ephemeral working drafts**, not project documentation. They are deleted when the feature is complete.

If you came here looking for "what does the codebase look like" or "why is X built this way", you are in the wrong directory — see [`../rationale/`](../rationale/) or [`../roadmap/`](../roadmap/).

## 0. The mental model

```
roadmap/      = where we want to go            (months — years, permanent)
rationale/    = why landed code is shaped so   (permanent, append-only)
agent-notes/  = how we are pushing this feature right now
                                               (hours — days, deleted when done)
```

`agent-notes/` is the only directory under `docs/` whose contents are **expected to disappear**.

## 1. When to create a plan file

**Default: do not create one.** Plans live in the conversation by default — proposed, accepted or rejected, executed, done.

A file under `agent-notes/` is created **only when the human explicitly says so**, with phrasing like:

- "把 plan 落成文档" / "把 plan 记下来"
- "plan 太长了，存一份"
- "我先动手，你把 plan 留个文档"

Do **not** create a plan file unprompted, even for long plans. The human decides when a plan deserves to be a document.

## 2. The collaboration workflow

```
human: "用 plan 模式计划 X"
   │
   ▼
agent: proposes plan in chat
   │
   ├── plan accepted as-is → execute, no file ever created
   │
   └── plan needs human-side iteration
       │
       ▼  (human says: "落成文档")
       agent writes docs/agent-notes/<file>.md
       │
       ▼
       human edits CODE while reading the plan
       (human does NOT edit the plan file)
       │
       ▼
       human: "我做了 A、B，看一下接下来怎么改 plan"
       │
       ▼
       agent re-reads file + diffs against HEAD,
       then rewrites Plan section + appends Notes
       │
       ▼  (loop until feature done)
       │
       human: "结束了" / "完成了" / "删掉"
       │
       ▼
       agent `git rm` the file in the same or next commit
```

## 3. The editing contract (most important rule)

| Artefact | Editor | Rule |
| --- | --- | --- |
| `Plan` section (the step list) | **agent only** | agent may rewrite, delete, add, check off steps freely between syncs |
| `Notes` section | **agent only** | append-only; record what the human did and how the plan was revised |
| `last-updated`, `status` frontmatter | **agent only** | bump on every plan edit |
| Project source code | human + agent | this is the actual work; the plan describes it, never replaces it |

The human **does not edit the plan file**. They express intent through chat; the agent reflects that intent into the file. This keeps a single source of truth for "current shared plan" and avoids merge-style conflicts between human prose and agent prose.

## 4. File naming

`docs/agent-notes/<YYYY-MM-DD>-<short-slug>.md`

- Date is the **creation date**, never updated.
- Slug is kebab-case, ≤ 5 words, describes the feature (`add-shader-hot-reload`, not `task1`).
- One file per feature. Do not split a feature across multiple plan files.

## 5. File structure (4 sections, fixed)

```markdown
---
task: <one sentence>
created: 2026-05-25
last-updated: 2026-05-25
status: in-progress | done-pending-cleanup
---

## Goal
What the human wants. 1–3 lines.

## Plan
- [ ] step 1
- [ ] step 2
- [x] step 3 (done by human / done by agent)
...

## Notes
Append-only log of revisions. Each entry is one short paragraph or bullet, prefixed with a date.
Example:
- 2026-05-25: human implemented original step 4 differently; rewrote step 4 → 4'.
- 2026-05-26: dropped step 5 (no longer needed after refactor).
```

No other top-level sections. No `Open Questions`, no `Cleanup` (those belong in chat or in this AGENTS.md respectively).

## 6. Re-entry protocol (every time agent picks up an existing plan)

Before discussing the next step, the agent **must**:

1. Read the plan file in full — do not rely on conversation memory; the human may have steered the chat away and back.
2. Diff the plan against HEAD: which steps are now actually implemented in code?
3. Update the `Plan` section: check off completed steps, rewrite or remove obsoleted ones.
4. Append a one-line entry to `Notes` summarizing what was reconciled.
5. Bump `last-updated`.
6. **Then** continue the conversation.

Skipping this protocol leads to plans that drift from reality, which defeats the entire purpose of the document.

## 7. Deletion (the only acceptable trigger)

The plan file is `git rm`-ed when, and only when, the human says one of:

- "结束了" / "完成了" / "done"
- "删掉 plan" / "可以删了"
- Any unambiguous variant of the above.

Do **not** auto-delete based on all-checkboxes-ticked. Do **not** ask "should I delete it?" repeatedly. The human is in charge of when a feature is over.

The deletion may share a commit with the final piece of implementation, or stand alone — pick whichever yields a cleaner history for that case.

## 8. Forbidden content

- ❌ Rationale prose ("why this design is good") — that is `../rationale/`'s job, after the feature has landed.
- ❌ Long-term ideas ("we should also do X someday") — that belongs in `../roadmap/`.
- ❌ Pasted code blocks longer than ~10 lines — point at file paths and line ranges instead.
- ❌ Verbatim chat transcripts.
- ❌ More than one feature per file.

## 9. Size discipline

There is no hard line cap, because long plans are exactly the use case for this directory. But keep individual sections breathable: if the `Plan` section exceeds ~100 lines, the feature is probably two features — split it before the file becomes a swamp.

## See Also

- [`../AGENTS.md`](../AGENTS.md) — top-level routing across `docs/`.
- [`../roadmap/AGENTS.md`](../roadmap/AGENTS.md) — long-horizon vision protocol.
- [`../rationale/AGENTS.md`](../rationale/AGENTS.md) — landed-history append-only protocol.
