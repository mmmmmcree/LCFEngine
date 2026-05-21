---
module: modern_rhi
scope: vision
last-anchor-commit: 755fc72
---

# Modern RHI — Overview

> A record-then-translate render hardware interface, not a Vulkan wrapper.

## Current State

The engine talks to Vulkan through hand-written code that mixes high-level intent (draw a frame) with low-level mechanics (queue submission, descriptor sets, pipeline barriers). There is no API-agnostic recording layer; cross-API portability would require rewriting most of `RenderEngine/`. Resource state tracking is implicit and scattered across call sites.

## Target Design

- **Two layers, sharply separated.** Front-end: a CPU-side *command recorder* that produces a typed, allocator-backed command stream. Back-end: per-API translators (Vulkan first, DX12 / Metal later) that consume the stream and emit native calls.
- **Commands are POD-ish.** No virtuals, no per-call heap allocs; the recorder owns a frame arena.
- **Resource handles are opaque indices.** The front-end never sees `VkImage`. Backends own the native objects keyed by handle.
- **State tracking lives in the backend.** The front-end records *intent*; the backend infers barriers, layout transitions, and queue ownership.
- **Multi-threaded recording is first class.** Per-thread recorders, deterministic merge at submit time. No locks on hot paths.
- **Submit is explicit and coarse.** A single `submit(stream, target)` call per pass; the backend may split, reorder, or merge passes as long as observable order is preserved.
- **Validation is a separate decorator backend.** No `#ifdef DEBUG` validation interleaved with production code paths.

## Gap vs Code

| Area | Today | Target |
| --- | --- | --- |
| API coupling | Vulkan calls scattered through RenderEngine | All Vulkan calls live behind one backend translator |
| Recording | Direct `vkCmd*` calls on a `vk::CommandBuffer` | Typed command stream with backend-agnostic verbs |
| Barriers | Manual / implicit | Backend-inferred from recorded resource access |
| Multi-thread | Single-threaded record | Per-thread recorders, deterministic merge |
| Cross-API | Vulkan only | Vulkan + DX12 + Metal share front-end |

## Deep-Dive Index

| File | One-line summary |
| --- | --- |
| _(none yet)_ | First deep-dive should be `command-recorder-design.md`. |

## Planned Deep-Dives (TODO)

- [ ] `command-recorder-design.md` — command stream encoding, arena allocation, per-thread recorders.
- [ ] `backend-translation-strategy.md` — recorded stream → native API; Vulkan first, DX12/Metal contract notes.
- [ ] `resource-state-tracking.md` — automatic barrier inference, queue ownership transfers, alias analysis.
- [ ] `pso-and-shader-integration.md` — how `shader_system` packages bind to RHI PSO objects (cross-module contract goes through PRINCIPLES).
- [ ] `validation-decorator-backend.md` — separate validation backend, capture/replay support.
- [ ] `multi-queue-submission.md` — graphics/compute/transfer queues, async compute scheduling.

## Open Questions

- Should the front-end model render passes / dynamic rendering uniformly, or expose both as recorded verbs?
- Where does the line sit between "recorder" and "render graph"? Are they the same thing or stacked?
- Is per-frame GC of the command arena cheap enough at our target draw counts, or do we need pooled segments?

## Changelog

- 2026-05-21 755fc72: initial overview, no deep-dives yet
