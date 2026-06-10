---
parent: modern_rhi
title: per-frame-resource-isolation
last-anchor-commit: f1cd84f
---

# Per-Frame Resource Isolation

## Problem

Single `VkBuffer` reused across frames-in-flight (current `VulkanBufferObject` shape) creates implicit GPU-side dependencies: frame N's CPU write must wait for frame N-1's GPU read, collapsing N-frame parallelism back to one. Mapped writes also induce cache-line ping-pong. Across-frame dependency must be eliminated by physical separation, not by barriers.

## Design

### Invariant

Frame N has **zero** dependency on N-1, N-2, ... beyond same-queue timeline monotonicity. CPU pacing waits the timeline target of frame **N-K** (K = frames-in-flight), not N-1. Violating this means in-flight=3 silently degrades to in-flight=1.

### Resource classes

| Class | Storage | Sync |
| --- | --- | --- |
| **Per-frame writable** (camera, instances, visible list, DGCC sequence) | K independent VkBuffers, persistent-mapped | None — slot N pacing only sees its own N-K target |
| **Shared read-only** (vertex/index, textures, materials) | Single VkBuffer/VkImage, read-only after upload | None |
| **Bindless buffer DS (set=1)** | K independent DS copies | Pending update flushed at slot's frame-begin |

Bindless keeps per-frame writable footprint minimal — only transforms + per-instance metadata + BDA tables. Typical < 1 MB × K.

### Layer placement

- **vk_core**: provides `Buffer` (single VkBuffer + mapped accessor + BDA). No frame concept, no slot ring.
- **render**: owns `PerFrameBuffer<T>` = `std::array<vkc::Buffer, K>` plus a `FrameRingState` that holds `m_submitted_target[K]` and the `currentSlot()` accessor.
- Pacing wait at frame begin: `queue.timeline().tryWaitFor(m_submitted_target[slot])`. K = render-layer constant.

### Per-frame command buffer

Same rule applies to CBs: each slot has its own CB, recorded once per slot at the slot's first use, reused across frames N, N+K, N+2K. Invalidated only on structural changes (PSO / bindless DS swap / attachments / DGCC layout). Invalidation rides vk_core's timeline-keyed `RetireQueue` (see the `vk_core` module).

### Verification

Toggle K = 1 vs K = 3, measure GPU utilization. Identical → some "per-frame" resource is actually shared writable. Audit by grepping for buffers with both `vkCmdCopyBuffer` writers and graphics-queue readers.

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| K independent VkBuffers per writable resource | Physical isolation, zero cross-frame dep | K× memory for writable data — small with bindless | ✅ chosen |
| Single VkBuffer with K offset segments | One alloc | Driver may insert barriers; cache aliasing | ❌ rejected |
| Single VkBuffer + manual barriers | Minimal memory | Defeats N-frame parallelism by design | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — `vkc::Buffer` (single buffer, no ring). Replace `VulkanBufferObject`'s implicit ring.
- [ ] Phase 2 — `render::PerFrameBuffer<T>` + `FrameRingState`; pacing wait against `m_submitted_target[slot]`.
- [ ] Phase 3 — Migrate camera UBO, InstanceData, VisibleInstances, DGCC sequence to PerFrameBuffer.
- [ ] Phase 4 — Per-slot CB allocation; invalidation on structural-change events.
- [ ] Phase 5 — Verification harness toggling K, asserting GPU-utilization scaling.

## Changelog

- 2026-06-08 f1cd84f: created — N-frame isolation invariant, render-layer multi-buffering, vk_core stays frame-agnostic
