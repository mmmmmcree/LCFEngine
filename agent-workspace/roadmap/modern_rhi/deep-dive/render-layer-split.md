---
parent: modern_rhi
title: render-layer-split
last-anchor-commit: f1cd84f
---

# Render Layer Split

## Problem

The overview's earlier two-layer split (`libs/gfx/{frontend, backend/vulkan}`) elides a third layer that already exists in `libs/vk_core/` and is needed independently of the render path. Backends translate engine intent (passes, draws, barriers) and own state tracking — they should not also own SDK init, queue management, swapchain, or VMA wrapping. Lumping those into "backend" makes vk_core's primitives unreusable for tools (asset baker, headless compute, validation utilities) and lets backend code casually reach for blocking sync APIs that vk_core forbids (PRINCIPLES §6.7). The boundary needs to be three-layer, not two.

## Design

### Three layers, each with a single responsibility

```
libs/render/frontend/        ← CPU command stream, API-agnostic
        │   typed POD verbs + opaque handles (frame-arena allocated)
        ▼
libs/render/backend/<api>/   ← Translation + state tracking
        │   native handles + Submission + RetireQueue
        ▼
libs/<api>_core/             ← Thin SDK substrate (vk_core today)
            │   SDK calls
            ▼
         Vulkan / DX12 / Metal SDK
```

- **`render/frontend`** records typed commands (`Draw`, `Dispatch`, `BeginPass`, `BindResources`, …) into a per-thread arena, parameterized by opaque handles. Has no `vk::` / `D3D12_*` / `MTL*` types. Multi-thread record is first-class; merge is deterministic at submit.
- **`render/backend/<api>`** translates the recorded stream into native calls, infers barriers/layout transitions, manages queue ownership, and resolves opaque handles to native resources. Owns `<api>`-specific objects that are *render-shaped* (PSO cache, render-pass cache, descriptor allocator strategy).
- **`<api>_core`** owns the SDK substrate: handles, init, sync primitives, memory, command pools, swapchain. Engine-policy-free per PRINCIPLES §6.7; its contracts are detailed in the sibling `vk_core` roadmap module.

### Boundary contracts

| Boundary | What may cross | What must not |
| --- | --- | --- |
| frontend → backend | typed command verbs, opaque handles, resource descriptors | `vk::*`, `ID3D12*`, `MTL*` types; backend-specific enums |
| backend → api_core | native handles, `Submission`, `RetireQueue`, `CommandRecorder`, `DeviceContext` accessors | scene/material/mesh types; ECS components; engine policy enums |
| api_core → SDK | full SDK surface (within the non-blocking contract) | nothing flows back up that names engine concepts |

A header file may be included only by its own layer and the layer immediately above. Frontend cannot include api_core; api_core cannot include backend. Compiler-enforced via include directories.

### Why api_core sits below backend, not inside it

Three concrete reuses justify the split:

1. **Tools** (asset baker, shader compiler harness, GPU profiler) need init + memory + sync but never touch render verbs.
2. **Headless compute** (offline bake, CI tests) wants `DeviceContext` + `CommandRecorder` without a swapchain or render passes.
3. **Cross-API porting** lets dx12_core and metal_core mirror vk_core's *contract* (non-blocking, leaf-handles, `RetireQueue`) while the backends mirror each other's *translation logic*. Two axes vary independently — splitting them keeps the diff per port small.

If api_core were folded into the backend, every tool would link the render backend to get a `vk::Device`. That ruins binary size and inverts dependencies.

### Where current code lands

| Today | Lands in |
| --- | --- |
| `RenderEngine/` frame loop, render targets, materials, scene-facing render API | `libs/render/frontend/` |
| `RenderEngine/Vulkan/` PSO assembly, descriptor managers, pass cache, barrier policy | `libs/render/backend/vulkan/` |
| `RenderEngine/Vulkan/` `VulkanContext`, `VulkanSwapchain`, `VulkanCommandBufferObject`, `VulkanBufferObject`, `VulkanImageObject`, sync primitives, VMA wrapper | `libs/vk_core/` |
| `libs/shader_core/` | unchanged — sits **beside** render, consumed by both frontend (reflection) and backend (binding) |

### Build-profile selection

Compile-time backend pick (CMake option → `if constexpr`) stays the single source of truth (see [`vulkan-extension-strategy`](./vulkan-extension-strategy.md)). The profile gates which `render/backend/<api>/*.cpp` is compiled and which `<api>_core` library is linked. Runtime-switchable mode (tools/editor) is opt-in and adds a `std::variant<VulkanBackend, …>` at the frontend↔backend seam — never inside api_core.

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Three-layer (frontend / backend / api_core) | Tools reuse api_core; non-blocking contract enforced at boundary; per-port diff bounded | One more directory, one more boundary to police | ✅ chosen |
| Two-layer (frontend / backend, backend embeds SDK substrate) | Fewer layers | Tools drag in render backend to use SDK; non-blocking contract becomes advisory | ❌ rejected |
| One-layer (RenderEngine talks SDK directly, status quo) | No migration work | Already failed: god-object context, blocking sync everywhere | ❌ rejected |
| Make api_core a runtime plugin | Maximum flexibility | Plugin boundary destroys `if constexpr` zero-cost dispatch; no actual use case requires it | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — Create empty `libs/render/frontend/` and `libs/render/backend/vulkan/` skeletons; lock include directories so cross-layer reach is a compile error.
- [ ] Phase 2 — Migrate SDK-substrate code from `RenderEngine/Vulkan/` into `libs/vk_core/` per the `vk_core` module's deep-dives.
- [ ] Phase 3 — Move PSO/descriptor/barrier-policy code from `RenderEngine/Vulkan/` into `libs/render/backend/vulkan/`. Backend depends only on `vk_core` + `shader_core`.
- [ ] Phase 4 — Define frontend command verb set and arena recorder; port one render pass end-to-end through the new stack as PoC.
- [ ] Phase 5 — Drop `RenderEngine/` once all call sites consume `libs/render/frontend/` directly; the directory either becomes the application shell or is deleted.

## Changelog

- 2026-06-08 f1cd84f: created — three-layer split (frontend / backend / api_core), boundary contracts, migration mapping
