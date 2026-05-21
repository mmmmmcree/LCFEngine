---
module: ecs_async
scope: vision
last-anchor-commit: 755fc72
---

# ECS + Async — Overview

> EnTT, Taskflow, and coroutines as one coherent scheduling fabric.

## Current State

The engine uses EnTT for entity/component storage and Taskflow as the executor. The wiring exists — `Registry` injects a `TaskScheduler` and an event `Dispatcher` into the EnTT registry context (`core/src/ecs/Registry.cpp:10`) and a Taskflow→asio coroutine bridge is in place (`core/src/tasks/Taskflow.cpp:13`). But systems still iterate sequentially: `TransformSystem::update` calls `view<Transform>().each()` directly (`Systems/src/TransformSystem.cpp:43`), and the per-frame loop in benchmarks runs `Dispatcher.update()` then `transform_system.update()` serially (`examples/gpu_driven_benchmark/src/main.cpp:702`). There is no scheduler-aware system declaration, no automatic parallelization, and event dispatch is single-threaded.

## Target Design

- **Systems are declarative.** Each system declares its component reads/writes; the scheduler builds a DAG and parallelizes non-conflicting systems automatically.
- **Per-archetype parallelism.** Within a system, iteration over a view is split across worker threads via Taskflow `for_each`, with chunk size tuned to component density.
- **Event dispatch is async.** Signals enqueue handlers onto the task graph; consumers do not block producers.
- **Coroutines are first-class for I/O-shaped work.** Asset loading, GPU readback, network — all expressed as `task<T>` co_await chains, scheduled by the same executor.
- **Frame is a `tf::Taskflow`.** The per-frame DAG is built once and re-executed; topology only rebuilds when system membership changes.
- **No bare `std::thread` outside the executor.** Adheres to PRINCIPLES §6.4.
- **Determinism opt-in.** A debug mode forces single-threaded execution to bisect non-deterministic bugs.

## Gap vs Code

| Area | Today | Target |
| --- | --- | --- |
| System scheduling | Manual sequential `update()` calls | Declared read/write set → automatic DAG |
| Within-system parallelism | `view.each()` on caller thread | Taskflow `for_each` with chunked archetypes |
| Event dispatch | Synchronous `Dispatcher.update()` | Async enqueue, handlers as task nodes |
| Coroutines | Plumbing exists, unused for game code | I/O-shaped systems express as `task<T>` |
| Per-frame DAG | Reconstructed each frame implicitly via call order | One persistent `tf::Taskflow`, rebuilt on schema change |

## Deep-Dive Index

| File | One-line summary |
| --- | --- |
| _(none yet)_ | First deep-dive should be `system-scheduling-model.md`. |

## Planned Deep-Dives (TODO)

- [ ] `system-scheduling-model.md` — read/write declaration, DAG construction, conflict detection.
- [ ] `archetype-parallel-iteration.md` — chunking strategy, work-stealing interaction, false-sharing avoidance.
- [ ] `async-signal-dispatch.md` — replace synchronous `Dispatcher.update()` with task-graph enqueue, ordering guarantees.
- [ ] `coroutine-integration-patterns.md` — when to use `task<T>` vs a system, executor binding, cancellation.
- [ ] `determinism-and-debug-modes.md` — single-thread fallback, frame replay support.

## Open Questions

- Do we declare read/write sets in code (templated) or in metadata (data-driven)? Tradeoff: compile-time safety vs hot-reloadable schema.
- How do we represent cross-frame data dependencies (e.g. GPU readback feeding next-frame logic) in the DAG?
- Should event handlers be ECS systems themselves, or a separate concept?

## Changelog

- 2026-05-21 755fc72: initial overview anchored on Registry + Taskflow bridge
