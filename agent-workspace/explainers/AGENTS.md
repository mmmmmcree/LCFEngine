# AGENTS.md — Protocol for Explainer / Onboarding Notes

This directory holds **explainer documents** that an agent writes to help the human understand a subsystem the agent worked on. They are pedagogical, not architectural — the audience is "the human three weeks from now who half-remembers building this".

If you came here looking for:

- "Why is the landed code shaped this way?" → `../rationale/`
- "Where do we want to go?" → `../roadmap/`
- "What is the agent doing right now?" → `../agent-notes/`
- "How does subsystem X work, with code anchors?" → **here**

## 0. The mental model

```
roadmap/      = the future, vision form
rationale/    = the past, why landed code stuck
agent-notes/  = the present, ephemeral plans
explainers/   = a bridge — code reading guides written for the human
```

Where the others answer "what should we do" / "why did we do it" / "what are we doing", explainers answer **"how do I read this code"**. Same feature can have an explainer here AND a rationale entry over there — they serve different audiences.

## 1. When to create a file

Only when the human explicitly asks for:

- "写一份原理文档 / 设计说明 / 源码导读"
- "解释一下 X 是怎么工作的"
- "写一份能帮我理解 Y 的文档"

Do NOT create unprompted. If the explanation fits in chat, keep it in chat.

## 2. File structure (loose, but every doc should answer 4 questions)

There is no rigid template. A good explainer covers, in any order:

1. **What problem does this solve** — the original pain, in 2–4 sentences.
2. **The mental model** — one diagram or analogy that makes the rest click.
3. **The code path** — actual files and line ranges; readers should be able to follow along in their editor.
4. **What is left out / future work** — what the first version deliberately does not do.

Optional but encouraged:

- Comparison to a well-known prior art (ninja, UE DDC, etc.) when it helps the human map known concepts onto our code.
- Worked examples: "if you change file X, this is what happens step by step."

## 3. Editing rules

- Anyone (agent or human) may edit. No append-only law.
- When the underlying code changes meaningfully, **update the explainer or delete it**. A stale explainer is worse than none — it teaches the wrong thing.
- Cite code with `path:line-range` so staleness is detectable by `grep`.
- Never paste large code blocks (> 15 lines). Quote the minimum needed and link to the file.

## 4. Naming

`agent-workspace/explainers/<topic-slug>.md` — kebab-case, no date prefix (these are not ephemeral).

## 5. Forbidden content

- ❌ Tutorial fluff ("As you can see, …")
- ❌ Prescriptive design rules — that is `roadmap/`'s job
- ❌ Claims about *why* a design was chosen vs alternatives — that is `rationale/`'s job (after landing)
- ❌ Verbatim chat transcripts
- ❌ Code dumps; quote, do not copy

## 6. Size

No hard cap. But if a single explainer exceeds ~400 lines, split it: probably it covers two subsystems.

## See Also

- [`../AGENTS.md`](../AGENTS.md) — top-level routing across `agent-workspace/`
- [`../rationale/AGENTS.md`](../rationale/AGENTS.md) — for the "why this design landed" record
- [`../roadmap/AGENTS.md`](../roadmap/AGENTS.md) — for forward-looking visions
