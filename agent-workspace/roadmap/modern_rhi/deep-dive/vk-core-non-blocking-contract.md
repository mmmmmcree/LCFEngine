---
parent: modern_rhi
title: vk-core-non-blocking-contract
last-anchor-commit: f1cd84f
---

# vk_core Non-Blocking Contract

## Problem

CPU↔GPU stalls in the current code surface as `vk::Device::waitIdle()` in destructors (`RenderEngine/src/Vulkan/VulkanContext.cpp:25`), implicit fence waits (`VulkanCommandBufferObject::waitUntilAvailable`), and the default-blocking shape of `TimelineSemaphore::wait()` (`libs/vk_core/src/sync/TimelineSemaphore.cpp:38-48`). When stall primitives are convenient, they get used reflexively, defeating multi-queue overlap. `vk_core` is also the wrong layer to decide *when* to stall — it owns Vulkan handles, not frame-loop policy. The contract must push synchronization decisions upward and make blocking calls visibly inconvenient.

## Design

### Hard rule: vk_core does not block

`vk_core` exposes only non-blocking primitives plus monotonic queries. Stall policy lives in `RenderEngine`. The single tolerated exception is `~DeviceContext()`, where teardown semantics require a final drain — documented as a destruction contract, not a usable API.

### Forbidden in vk_core public surface

| Symbol | Reason |
| --- | --- |
| Public `vk::Device::waitIdle()` wrapper | Whole-device stall; destroys queue parallelism |
| Public `vk::Queue::waitIdle()` wrapper | Per-queue stall; same problem at smaller scale |
| `TimelineSemaphore::wait()` as default name | Default-named API gets used by reflex; blocking must be opt-in lexically |
| `vkAcquireNextImageKHR(timeout = UINT64_MAX)` | Implicit frame-pacing stall; swallows present-engine policy |
| `waitForFences` anywhere | Timeline is the baseline (`vulkan-extension-strategy.md`); `VkFence` is not in the new layer |

### Required shapes

- **Sync primitive:** `TimelineSemaphore::isReached()`, `currentValue()`, `tryWaitFor(deadline)` returning `bool`. Any blocking variant is named `waitBlocking()` and tagged `[[nodiscard]]` so accidental use stands out.
- **Submission:** `Submission` carries CB list + wait/signal semaphores + retire timeline value. `QueueSubmitter::submit(Submission)` returns the signaled timeline value. It does not wait.
- **Acquire/present:** swapchain exposes `tryAcquire(timeout = 0)` returning `expected<AcquiredImage, AcquireError>`. `OutOfDate`/`Suboptimal` are values, not exceptions; the caller decides recreate timing.
- **Resource retirement by timeline value:**

```cpp
class RetireQueue {
    std::deque<std::pair<uint64_t, std::function<void()>>> m_pending;
public:
    void retire(uint64_t target_value, std::function<void()> deleter);
    void tick(uint64_t reached_value);   // RenderEngine calls per frame
};
```

  Replaces fence-waited deletion. `RenderEngine` queries `submitter.signaledValue()` once per frame and calls `tick()` — pure non-blocking poll.

### What vk_core provides

| Category | Contents |
| --- | --- |
| Context | `InstanceContext`, `DeviceContext`, `CommandContext` (see [`context-decomposition`](./context-decomposition.md)) |
| Sync | `TimelineSemaphore`, `FrameTimeline` (monotonic counter; query-only) |
| Memory | VMA wrapper, `StagingArena` (persistently mapped, segmented by timeline value) |
| Resources | `Buffer`, `Image`, `Sampler` — value types, handle-only |
| Recording | `CommandRecorder`, `BarrierBatch` builder |
| Submission | `Submission`, `QueueSubmitter::submit` (non-blocking) |
| Pools | `CommandPoolSet` (per-thread × per-family), reset by timeline value |
| Retire | `RetireQueue<T>` — deferred deleter keyed by timeline value |
| Descriptors | Layout cache, allocator |
| Shader | SPIR-V module / shader object loader (build-profile gated) |
| Swapchain | Non-blocking acquire/present (see [`swapchain-gui-decoupling`](./swapchain-gui-decoupling.md)) |

### What vk_core deliberately delegates upward

Frame loop, render graph, render pass orchestration, frame pacing, descriptor manager, sampler manager, swapchain registry. These need engine policy; vk_core has none.

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Non-blocking surface + `RetireQueue` polled by frame loop | Forces stall policy into one place; multi-queue overlap stays intact; testable in isolation | Frame loop must remember to poll; first port of existing call sites needs rewrite | ✅ chosen |
| Hybrid (blocking helpers available "for convenience") | Easier porting | Convenience APIs win by default; contract erodes | ❌ rejected |
| Per-resource finalizer via `vkQueueSubmit2` callbacks | Decentralized; no central queue | No standard mechanism in Vulkan; would require thread-pool infra inside vk_core | ❌ rejected |
| Keep current shape, add lint rule for `waitIdle` | Minimal change | Lint catches one symbol; doesn't address default-blocking sync primitives | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — Rename `TimelineSemaphore::wait()` → `waitBlocking()`; add `isReached()`, `currentValue()`, `tryWaitFor()`. Mark blocking variant `[[nodiscard]]`.
- [ ] Phase 2 — Introduce `RetireQueue<T>` and `Submission`/`QueueSubmitter` skeletons under `vk_core/sync/` and `vk_core/queue/`.
- [ ] Phase 3 — Migrate `VulkanCommandBufferObject` to non-blocking submit; remove `waitUntilAvailable` from public API; resource leases retire via `RetireQueue` instead of fences.
- [ ] Phase 4 — Convert swapchain to `tryAcquire`/`tickRetire` (cross-references [`swapchain-gui-decoupling`](./swapchain-gui-decoupling.md) Phase 3).
- [ ] Phase 5 — Forbid public `waitIdle` wrappers; document `~DeviceContext()` as the only sanctioned drain point.

## Changelog

- 2026-06-08 f1cd84f: created — non-blocking contract, RetireQueue, vk_core scope boundary
