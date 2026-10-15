---
adr: 0002
parent: _archive
status: accepted
date: 2026-10-15
last-anchor-commit: d38f6c2
---

# ADR-0002: Add top-level playground directory

## Context

The user requested a runnable async library experiment under agent-workspace before moving the implementation into production libraries.
It needs its own CMake entry point, external consumer, verification and benchmarks.
The existing documentation directories do not provide an appropriate home for executable prototypes.

## Decision

Add agent-workspace/playground as the home for independently configurable executable experiments, beginning with playground/async.

## Consequences

- (+) A prototype can be opened and built without configuring the engine.
- (+) Code and its development workflow can be reviewed before production migration.
- (+) Each experiment keeps its usage instructions beside its CMake entry point.
- (−) Experimental copies can diverge from production code and must state their status explicitly.

## Alternatives Considered

- **Move directly into libs** — rejected because the user wants to understand and validate the development structure first.
- **Put executable projects in docs** — rejected because runnable source trees have different maintenance needs from prose.
- **Use agent-notes** — rejected because its task-lifetime lifecycle does not fit an experiment the user will continue running.
