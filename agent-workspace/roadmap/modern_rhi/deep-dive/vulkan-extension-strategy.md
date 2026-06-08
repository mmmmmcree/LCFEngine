---
parent: modern_rhi
title: vulkan-extension-strategy
last-anchor-commit: 04247ac
---

# Vulkan Extension Strategy

## Problem

The current `RenderEngine/` enables Vulkan extensions ad-hoc at device creation and then sprinkles `if (caps.hasFoo)` checks throughout the codebase. This produces three failure modes: (1) every feature must carry a slow legacy fallback even in builds where the feature is mandatory, (2) the binary cannot be reasoned about — what is actually compiled in is determined by runtime probes, (3) "engine mode" tooling needs (e.g. fast shader iteration via shader objects) leak into shipping game builds. The new `libs/gfx/backend/vulkan/` must collapse this matrix to a small number of statically-known build profiles.

## Design

### Two-axis configuration

- **Build axis (compile-time, CMake options → `constexpr` header).** Each Vulkan extension or feature group becomes one switch. Disabled = zero code, zero binary size, zero runtime branch.
- **Device axis (runtime probe, *validation only*).** When the build axis enables a feature, every target device must support it. A device that does not is rejected at init time. There is no per-extension fallback ladder inside backend hot paths.

```cmake
# build_configs/gfx_vulkan.cmake
option(LCF_GFX_VK_INDIRECT        "VK_KHR_draw_indirect_count etc." ON)
option(LCF_GFX_VK_SHADER_OBJECT   "VK_EXT_shader_object (engine/editor profile)" OFF)
option(LCF_GFX_VK_MESH_SHADER     "VK_EXT_mesh_shader" OFF)
# Timeline semaphores are baseline: not an option.
```

```cpp
// generated: libs/gfx/backend/vulkan/include/gfx/vk/Config.hpp
namespace gfx::vk::cfg {
inline constexpr bool kIndirect     = @LCF_GFX_VK_INDIRECT@;
inline constexpr bool kShaderObject = @LCF_GFX_VK_SHADER_OBJECT@;
inline constexpr bool kMeshShader   = @LCF_GFX_VK_MESH_SHADER@;
}
```

Backend code uses `if constexpr (cfg::kIndirect) { … }` — disabled paths vanish from the binary. Hot paths never read a runtime capability flag.

### "Highest supported" rule

When an extension is compiled in, the backend always selects the strongest variant the device exposes (e.g. prefer `VK_KHR_draw_indirect_count` over emulated multi-draw loops). Devices below that bar fail device selection — they are not usable for the build profile.

### Timeline baseline

Vulkan 1.2 Timeline Semaphores are mandatory. The new layer has **no `VkFence` path**. This collapses sync code to one model and removes a class of subtle ordering bugs. Any device/driver lacking timeline semaphores is unsupported.

### Shader Object vs Shader Module is a build profile

| Profile | Path | When |
| --- | --- | --- |
| `engine` / editor | `VK_EXT_shader_object` | Hot-reload, live shader authoring, no PSO cache to invalidate |
| `game` / runtime | `VkShaderModule` + PSO cache | Shipping builds, deterministic perf, broad device coverage |
| `legacy` | `VkShaderModule` only | Devices without shader-object support |

Selection is a single CMake variable (`LCF_GFX_SHADER_PIPELINE = engine|game|legacy`); it sets `kShaderObject` and gates which backend `.cpp` files compile. Mixed-mode builds are rejected at configure time.

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Compile-time switches + `if constexpr` | Zero-cost, statically auditable binary, no fallback ladders | Multiple build configs to ship; users must pick a profile | ✅ chosen |
| Runtime extension toggles everywhere | One binary fits all GPUs | Branchy hot paths, every feature carries a legacy twin, validation surface explodes | ❌ rejected |
| Mixed shader-object + shader-module per pipeline | Maximum flexibility | Doubles PSO-management code, two cache schemes, nontrivial bug surface | ❌ rejected |
| Drop Timeline baseline, keep `VkFence` fallback | Slightly broader device support | Two sync models forever; complicates multi-queue plans | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — Add `build_configs/gfx_vulkan.cmake`; generate `gfx/vk/Config.hpp`; wire Timeline-only sync.
- [ ] Phase 2 — Convert existing `RenderEngine/` extension probes into compile-time `if constexpr` sites under `libs/gfx/backend/vulkan/`.
- [ ] Phase 3 — Introduce `LCF_GFX_SHADER_PIPELINE` profile; split shader-object vs module code paths into separate TUs.
- [ ] Phase 4 — Device selection rejects devices that miss any compiled-in extension; remove all runtime fallback ladders.
- [ ] Phase 5 — Document the supported profile matrix in module README; CI builds one config per profile.

## Changelog

- 2026-05-29 04247ac: created — compile-time extension policy, Timeline baseline, shader object/module profiles
