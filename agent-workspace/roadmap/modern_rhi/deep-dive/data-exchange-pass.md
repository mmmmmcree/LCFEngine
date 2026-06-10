---
parent: modern_rhi
title: data-exchange-pass
last-anchor-commit: f1cd84f
---

# Data Exchange Pass

## Problem

CPU→GPU updates have three distinct frequencies; mixing them onto one path forces every update to pay the worst-case cost. Today, occasional updates (material edits, texture loads) ride the same graphics CB as per-frame updates, polluting the hot path with barriers and serializing what could overlap on the transfer queue.

## Design

### Three frequency tiers

| Tier | Examples | Path | Sync cost |
| --- | --- | --- | --- |
| **Per-frame** | camera, instance transforms, visible list | CPU → mapped per-frame buffer | None ([`per-frame-resource-isolation`](./per-frame-resource-isolation.md)) |
| **On-change** | material params, bindless texture upload | CPU → staging → transfer queue → device-local | Graphics waits transfer's timeline target only when something flushed |
| **Static** | vertex/index, fixed textures | Loader phase, before main loop | Drained at load time |

Hot path never enters the transfer queue. On-change is the only tier that does.

### DataExchangePass (render layer)

- `enqueueBufferWrite(dst, offset, bytes)` / `enqueueImageWrite(...)` — CPU memcpy into staging arena. Lock-free if staging is per-thread-segmented.
- `flush(transfer_queue) -> uint64_t` — submit one transfer CB with accumulated copies; returns `target` (0 if nothing queued).
- StagingArena segments retired by transfer queue's timeline (not frame slot — staging is queue-scoped).

### Frame loop stitching

```cpp
auto upload_target = data_exchange.flush(transfer_queue);  // often 0
auto sub = recordGraphics(slot);
if (upload_target != 0) sub.waits.push_back(waitOn(transfer_queue, upload_target));
auto graphics_target = graphics_queue.submit(std::move(sub));
```

`upload_target == 0` skips the wait entirely. When non-zero, the wait is GPU-side semaphore — CPU returns immediately; transfer CBs are short, so by the time graphics is ready to execute, the wait is already satisfied. Effectively zero blocking.

### Queue family ownership

Targets of DataExchangePass (MaterialParams, bindless upload destinations) use `VK_SHARING_MODE_CONCURRENT` for transfer + graphics families. Avoids release/acquire barrier pair; cost negligible at on-change traffic levels.

### Layer placement

- **vk_core**: `QueueContext`, `StagingArena` primitive (timeline-keyed segment recycling).
- **render**: `DataExchangePass` (enqueue API, copy descriptor accumulation, flush orchestration).

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Dedicated transfer queue + DataExchangePass | On-change overlaps with graphics; zero cost when idle; hot path untouched | Cross-queue sync to reason about (timeline solves it) | ✅ chosen |
| Stage + copy on graphics CB | Single queue, simpler | On-change traffic taxes hot path; no overlap | ❌ rejected |
| Always-on transfer wait every frame | Uniform shape | Wastes a semaphore wait in idle frames | ❌ rejected |
| EXCLUSIVE sharing + manual ownership transfer | Strict, possibly faster | Two-sided barriers; large code surface for marginal win | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — `vkc::StagingArena` (persistent mapped, timeline-keyed segments).
- [ ] Phase 2 — `render::DataExchangePass` enqueue + flush.
- [ ] Phase 3 — Frame loop stitching with conditional `waitOn(transfer)`.
- [ ] Phase 4 — Migrate MaterialParams + bindless texture uploads off graphics CB.
- [ ] Phase 5 — Verify hot path: graphics CB has no `vkCmdCopy*` in steady state.

## Changelog

- 2026-06-08 f1cd84f: created — three-tier frequency split, transfer-queue DataExchangePass, conditional GPU-side wait
