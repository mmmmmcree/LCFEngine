---
module: vulkan_optimizations
scope: short-term
last-anchor-commit: 755fc72
---

# Vulkan Optimizations — Overview

> Near-term, code-anchored wins on the existing Vulkan backend.

## Current State

The Vulkan renderer rings command buffers across `FRAME_OVERLAP=3` frames (`RenderEngine/src/Vulkan/VulkanRenderer.cpp:86`) but **does not** ring the per-renderable SSBOs. A single `VulkanBufferObjectGroup m_per_renderable_ssbo_group` is created once with `eDynamic` usage (`RenderEngine/src/Vulkan/VulkanRenderer.cpp:140`) and shared across all in-flight frames (`RenderEngine/include/Vulkan/VulkanRenderer.h:59`). Every frame writes into the same buffers via `commitAll(transfer_cmd)` (`RenderEngine/src/Vulkan/VulkanRenderer.cpp:579`), creating a write-after-write hazard that forces `cmd.waitUntilAvailable()` to serialize on the previous frame's timeline semaphore (`RenderEngine/src/Vulkan/VulkanRenderer.cpp:438`). The result is implicit GPU-CPU serialization that defeats the purpose of triple-buffered command submission. A pre-existing TODO acknowledges the buffer-versioning gap (`RenderEngine/include/Vulkan/memory/VulkanBufferObject.h:7`).

## Target Design

- **Ring per-frame SSBOs alongside command buffers.** `m_per_renderable_ssbo_group` becomes one instance per frame slot; commit writes into the slot owned by the current frame index.
- **Eliminate the prev-frame timeline wait** in the common path. The wait should only fire on resource resize, not on every frame.
- **Tighten interface boundaries.** `FrameResources` is the natural owner of all per-frame mutable GPU state; SSBOs move into it.
- **Audit other "single dynamic buffer" sites.** Any `eDynamic` buffer written every frame and read by GPU is a candidate for the same fix.
- **Track barriers explicitly per slot.** Multi-buffering only helps if the barrier graph stops referencing the previous frame's writes.
- **Telemetry for verification.** Add a `frame_serialization_stalls` counter that increments whenever the front of the queue waits on the back.

## Gap vs Code

| Area | Today | Target |
| --- | --- | --- |
| SSBO multi-buffering | Single shared instance, all frames | One per `FRAME_OVERLAP` slot, owned by `FrameResources` |
| Frame entry stall | Timeline wait every frame | Wait only on resize / resource change |
| Buffer ownership | Top-level renderer member | Per-`FrameResources` member |
| Stall observability | None | Counter exposed via profiling overlay |

## Deep-Dive Index

| File | One-line summary |
| --- | --- |
| _(none yet)_ | First deep-dive should be `per-frame-ssbo-ring.md`. |

## Planned Deep-Dives (TODO)

- [ ] `per-frame-ssbo-ring.md` — concrete refactor of `m_per_renderable_ssbo_group` into `FrameResources`.
- [ ] `dynamic-buffer-audit.md` — enumerate every `eDynamic` buffer, classify by writer/reader frequency, plan ring or staging.
- [ ] `barrier-tracking-cleanup.md` — once per-slot, prune cross-frame barriers; verify `waitUntilAvailable` no longer hot.
- [ ] `interface-convergence.md` — collapse overlapping APIs in `VulkanBufferObject` / `VulkanBufferObjectGroup`; remove the resize-versioning TODO.

## Open Questions

- Does `eDynamic` (host-visible, host-coherent) still make sense per-frame, or should we move to staging + `eDeviceLocal`?
- Is `FRAME_OVERLAP=3` the right hardcoded constant, or should it adapt to swapchain image count? (`RenderEngine/src/Vulkan/VulkanRenderer.cpp:86` flags this.)
- What's the cost of allocating `N×` of every dynamic buffer on memory-constrained targets?

## Changelog

- 2026-05-21 755fc72: initial overview anchored on observed serialization in VulkanRenderer
