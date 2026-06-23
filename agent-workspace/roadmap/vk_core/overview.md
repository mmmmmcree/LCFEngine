---
module: vk_core
scope: vision
last-anchor-commit: aaaa573
---

# vk_core (vkc) — Overview

> Engine-policy-free Vulkan SDK substrate.

## Current State

`libs/vk_core/` exists as a placeholder skeleton in namespace `lcf::vkc`: header stubs (~20 lines each) under `context/`, `memory/`, `descriptor/`, `pipeline/`, `shader/`, `surface/`, `sync/`. Only `TimelineSemaphore` has an implementation (`libs/vk_core/src/sync/TimelineSemaphore.cpp`), and its `wait()` is default-blocking — violating the target contract. The real working code still lives in `RenderEngine/Vulkan/`: `VulkanContext` is a god-object aggregate, the swapchain holds a `gui::VulkanSurfaceBridge` pointer, and teardown paths call `waitIdle()`.

## Target Design

- **Lowest layer of the three-layer render split** (PRINCIPLES §6.7). Consumed by `render/backend/vulkan` and by tools / headless compute. Knows nothing about engine policy, frames-in-flight, scenes, or ECS.
- **Leaf classes hold handles, not contexts.** `Buffer`, `Image`, `Swapchain`, `CommandBuffer` etc. store only the `vk::*` handles they call on; resources needed per-operation are passed at the call site. See [`context-decomposition`](./deep-dive/context-decomposition.md).
- **Three context lifetimes, strictly nested ownership.** `Context` (instance, debug, instance dispatch) owns n `DeviceContext`s (physical+logical device, capability snapshot, device dispatch, VMA); each owns its `QueueContext`s. `bs::bootstrap` returns `expected<Context, error_code>`.
- **Queue + Timeline is the only sync model** (PRINCIPLES §6.8). `QueueContext` solely owns its `vk::Queue`, `TimelineSemaphore`, and submission counter — the single submission funnel; raw queues are never exposed. `vk::SemaphoreSubmitInfo` is the one cross-layer sync token; `VkFence` does not appear in vkc.
- **Full topology + role index, at both levels.** Bootstrap creates every queue of every family (ownership: flat array per device). Deterministic collapse ladders map `enums::QueueRole` (eMainGraphics / eSubGraphics / eCompute / eTransfer) onto queues and `enums::DeviceRole` (eMain / eCompute) onto devices — dedicated hardware preferred, scarce machines alias several roles to one queue/device, safe because `QueueContext` is the single funnel. What each role is *used for* remains render-layer policy.
- **Per-module feature dependencies.** Each module ships `feature_dependencies.h` — its single dependency entry point (`vkc::<module>::k_module_dependency`, optional `k_xxx_dependency` children), analogous to `deps.cmake`. Callers pass the dependencies they use to `bs::bootstrap`, which dedups extensions and enables what the device supports into a capability snapshot; vkc never rejects devices — render-layer routing reads the snapshot. Namespaces mirror caller roles (`vkc` / `bs` / `conf` / `enums` / `details`). See [`bootstrap-and-feature-dependencies`](./deep-dive/bootstrap-and-feature-dependencies.md).
- **Buffer/Image are thin proxies over `details::Memory<vk::Buffer|vk::Image>`**, held via `ResourcePointer`, exposing vk-native usage semantics plus `vk::DeviceAddress` — no engine-level usage enums.
- **Non-blocking public surface.** No `waitIdle` wrappers, no default-blocking waits; deferred destruction via timeline-keyed `RetireQueue`. See [`non-blocking-contract`](./deep-dive/non-blocking-contract.md).
- **Zero `gui/` dependency.** Swapchain consumes a `SurfaceProvider` callback bundle and owns `vk::UniqueSurfaceKHR` itself. See [`swapchain-gui-decoupling`](./deep-dive/swapchain-gui-decoupling.md).
- **Frame-agnostic.** vkc provides single resources (`Buffer`, `StagingArena`); multi-buffering/ring policy belongs to the render layer.

## Gap vs Code

| Area | Today | Target |
| --- | --- | --- |
| Context | `VulkanContext` god object in `RenderEngine/` | `Context` / `DeviceContext` / `QueueContext` + free-function init |
| Extensions | ad-hoc enable + runtime `hasFoo` checks | manifest registry fold in `init::bootstrap`; capability snapshot |
| Leaf wrappers | hold `VulkanContext *` | hold handles only |
| Sync | `TimelineSemaphore` default-blocking, owns submission counter | stateless semaphore wrapper; counter in `QueueContext`; `isReached` / `tryWaitFor` / opt-in `waitBlocking` |
| Queues | role map `QueueFlagBits → index` in `VulkanContext` | full-topology `QueueTable`; `QueueContext` sole submit funnel |
| Swapchain | couples `gui::VulkanSurfaceBridge`; bridge owns surface | `SurfaceProvider` callbacks; vkc owns `vk::UniqueSurfaceKHR` |
| Memory | `VulkanBufferObject` with implicit frame ring | `Buffer`/`Image` over `details::Memory<T>`; `StagingArena` timeline-segmented |

## Deep-Dive Index

| File | One-line summary |
| --- | --- |
| [`bootstrap-and-feature-dependencies.md`](./deep-dive/bootstrap-and-feature-dependencies.md) | Per-module `feature_dependencies.h` entry points; explicit selection folded by `bs::bootstrap` into a `Context`; capability snapshot; namespace roles. |
| [`context-decomposition.md`](./deep-dive/context-decomposition.md) | Leaf classes hold handles only; context splits into role-typed providers; init becomes free functions returning bundles. |
| [`non-blocking-contract.md`](./deep-dive/non-blocking-contract.md) | vkc never blocks; forbidden-API list; `RetireQueue` keyed by timeline value; module scope boundary. |
| [`swapchain-gui-decoupling.md`](./deep-dive/swapchain-gui-decoupling.md) | `SurfaceProvider` callback bundle; vkc owns `vk::SurfaceKHR`; pull+push resize notification. |
| [`pipeline-rendering-abstraction.md`](./deep-dive/pipeline-rendering-abstraction.md) | 4-class two-axis model (Static/Dynamic × Pipeline/Rendering), 3 legal pairings, one Info drives both, unified `begin(cmd)`. |

## Planned Deep-Dives (TODO)

- [ ] `memory-and-staging.md` — `details::Memory<T>`, `ResourcePointer` lifecycle, VMA wrapping, `StagingArena` segment recycling.
- [ ] `command-recording.md` — `CommandRecorder`, `CommandPoolSet` (per-thread × per-family), `SubmitBatch`/`QueueContext::submit` shapes.
- [ ] `error-handling.md` — `expected<T, error_code>` conventions, when to assert vs return.

## Open Questions

- `DescriptorHeap`/`DescriptorSet` scope: does vkc own allocation strategy, or only raw pool/layout wrappers with strategy upstairs?
- `QueueContext::submit` thread safety: internal lock, or documented external synchronization?
- `RetireQueue` granularity: one per `QueueContext`, or one global tracking multiple queues?
- Present ownership: `Swapchain::present(QueueContext &)`, or a present API on `QueueContext`?

## Changelog

- 2026-06-23 a856fe6: add pipeline-rendering-abstraction deep-dive (Static/Dynamic × Pipeline/Rendering, unified begin(cmd))
- 2026-06-10 aaaa573: QueueRole collapse-ladder index over full topology; devices sorted by score
- 2026-06-10 aaaa573: bootstrap returns Context; nested Context→DeviceContext→QueueContext ownership
- 2026-06-10 aaaa573: explicit feature_dependencies selection replaces registry (decisions/0001)
- 2026-06-10 aaaa573: Queue+Timeline ownership, manifest mechanism, namespaces; add bootstrap deep-dive
