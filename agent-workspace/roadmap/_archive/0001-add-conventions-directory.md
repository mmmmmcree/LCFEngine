---
adr: 0001
parent: _archive
status: accepted
date: 2026-06-10
last-anchor-commit: aaaa573
---

# ADR-0001: Add top-level `conventions/` directory

## Context

Code style corrections from the human were accumulating only in chat history and the agent's private session memory — invisible to other agents, unauditable, and prone to divergence from what the human actually confirmed. The existing four directories all have the wrong shape for this content: roadmap is future vision, rationale is append-only history, agent-notes is ephemeral, explainers is pedagogy. Style rules are present-tense, durable, and must be *iterated in place* as feedback arrives.

## Decision

Add `agent-workspace/conventions/` as a fifth top-level directory holding living convention documents (starting with `cpp-style.md`), updated by the agent whenever the human gives style feedback.

## Consequences

- (+) Style feedback survives across sessions and agents; single source of truth.
- (+) Feedback loop is explicit: human corrects → agent edits the doc in the same task.
- (+) Conventions are reviewable/diffable like any other workspace document.
- (−) One more directory to route; one more read before writing code.
- (−) Potential overlap with `CLAUDE.md` conventions — resolved by precedence rule in `conventions/AGENTS.md`.

## Alternatives Considered

- **Agent private memory only** — rejected because it is invisible to the human and other agents, and cannot be reviewed or corrected in place.
- **Extend CLAUDE.md directly** — rejected because CLAUDE.md is a broad project-instruction surface; high-churn style rules need their own iteration cadence and protocol.
- **Put rules in `explainers/`** — rejected because explainers teach existing code, they do not prescribe.
