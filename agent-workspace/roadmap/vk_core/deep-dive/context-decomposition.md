---
parent: vk_core
title: context-decomposition
last-anchor-commit: aaaa573
---

# Context Decomposition

## Problem

`RenderEngine/include/Vulkan/VulkanContext.h` aggregates eight unrelated responsibilities (instance, physical device, device, queue map, command pool map, memory allocator, descriptor manager, sampler manager). Every leaf wrapper takes a `VulkanContext *` and thereby gains transitive access to all of them — a textbook god object. The same shape is replicated in `VulkanCommandBufferObject` (`m_context_p` member, `RenderEngine/include/Vulkan/VulkanCommandBufferObject.h:34`), even though the class only reads queue, pool, and timeline. The construction code in `VulkanContext.cpp:40-53` is a sequence of side-effecting member calls whose state is entirely outputs — an unrolled procedure trapped inside a class.

## Design

### Leaf classes hold handles, not contexts

Vulkan handles (`vk::Device`, `vk::Queue`, `vk::CommandPool`, `VmaAllocator`) are 8-byte opaque values; copying them is free and lifetime is owned upstream. A leaf wrapper stores **only the handles it actually calls on**, plus its own metadata. No `Context *` member, no `DeviceContext *` member, no equivalent aggregate.

Invariant (proposed for vk_core PRINCIPLES):
> Any leaf class in `vk_core` may hold Vulkan handles and metadata. It may **not** hold a pointer or reference to any aggregate context. Resources required for an operation are passed in at the call site.

This rule applies uniformly to `Buffer`, `Image`, `DescriptorSet`, `Pipeline`, `Sampler`, `CommandBuffer`. The new `VulkanDescriptorSet2` (no `m_context_p`) is the reference implementation.

### Context split into three role-typed providers

```cpp
class InstanceContext { /* vk::UniqueInstance + debug messenger + loaded ext */ };
class DeviceContext   { /* vk::PhysicalDevice + vk::UniqueDevice + QueueTable + caps + VmaAllocator */ };
class CommandContext  { /* QueueRef + vk::CommandPool + TimelineSemaphore& */ };
```

`DeviceContext` is the read-only handle provider for leaf-class call sites. It does not own descriptor managers, sampler managers, or render targets — those are higher-level services composed by `RenderEngine`. `CommandContext` replaces the `VulkanContext *` currently held by `VulkanCommandBufferObject` and carries exactly what record/submit needs.

### `create()` becomes free functions returning bundles

Init is procedural. `vk_core::init` exposes pure functions taking config structs and returning `expected<Bundle, error_code>`:

```cpp
[[nodiscard]] expected<InstanceBundle, error_code> createInstance(const InstanceConfig &);
[[nodiscard]] expected<DeviceBundle,   error_code> createDevice(vk::Instance, const DeviceConfig &);
```

`InstanceConfig` and `DeviceConfig` are POD-ish structs (extension list, queue requirements, feature requirements, scorer). `DeviceContext` becomes a thin owner that takes the two bundles in its constructor and exposes getters — it contains no logic.

### What moves where

| Currently in `VulkanContext` | Lands in |
| --- | --- |
| Instance / physical device pick / device create | `vk_core::init` free functions |
| Queue table, command pools, VMA | `DeviceContext` (data only) |
| `DescriptorSetManager`, `SamplerManager` | RenderEngine higher-level service composition |
| `m_surface_render_targets` | RenderEngine (swapchain registry is not device-level state) |

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Leaf holds handles only; Context is provider | Explicit dependencies, leaf wrappers movable/testable, no transitive god-object access | Leaf ctors gain extra params (`vk::Device`, `vk::CommandPool`, …) | ✅ chosen |
| Keep single Context, restrict via concepts | Minimal diff | Dependency still implicit; concept-based DI is verbose; god object survives | ❌ rejected |
| `create()` as member methods on Context | Familiar OOP shape | Init logic permanently coupled to Context type; headless / partial init impossible | ❌ rejected |
| Throw-on-error ctor | Less plumbing | Cannot recover stage-by-stage; tools/editor want graceful fallback | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — Add `vk_core/context/{InstanceContext,DeviceContext,CommandContext}.h` as data-only owners; no init logic inside.
- [ ] Phase 2 — Extract `vk_core/init/{createInstance,createDevice}` as free functions returning `expected<Bundle, error_code>`. Move logic out of `VulkanContext.cpp:75-274`.
- [ ] Phase 3 — Rewrite `VulkanCommandBufferObject` to hold `(vk::Device, vk::CommandPool, QueueRef, TimelineSemaphore *)`; drop `m_context_p`. PoC for the leaf-class invariant on the most coupled wrapper.
- [ ] Phase 4 — Migrate remaining leaves (`VulkanBufferObject`, `VulkanImageObject`, `VulkanSampler`) to the same shape.
- [ ] Phase 5 — Delete `RenderEngine::VulkanContext`; relocate `DescriptorSetManager`/`SamplerManager` to RenderEngine service layer.

## Changelog

- 2026-06-10 aaaa573: moved into vk_core module
- 2026-06-08 f1cd84f: created — context split, leaf-handles-only invariant, procedural init
