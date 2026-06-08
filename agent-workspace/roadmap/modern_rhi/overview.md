---
module: modern_rhi
scope: vision
last-anchor-commit: f1cd84f
---

# Modern RHI — Overview

> A record-then-translate gfx layer, not a Vulkan wrapper.

## Current State

The engine talks to Vulkan through hand-written code in `RenderEngine/` that mixes high-level intent (draw a frame) with low-level mechanics (queue submission, descriptor sets, pipeline barriers). There is no API-agnostic recording layer; cross-API portability would require rewriting most of `RenderEngine/`. Resource state tracking is implicit and scattered across call sites. Vulkan extensions are toggled ad-hoc at instance/device creation with no central capability policy.

## Target Design

- **Three layers.** `libs/render/frontend/` records an API-agnostic CPU command stream (typed POD verbs, frame-arena allocated). `libs/render/backend/<api>/` translates that stream — owns barrier inference, PSO/descriptor policy, queue ownership. `libs/<api>_core/` is the SDK substrate underneath (handles, init, sync, memory, swapchain) — engine-policy-free and reusable by tools / headless compute. See [`render-layer-split`](./deep-dive/render-layer-split.md).
- **Backend selection is layered.** *Compile-time* by default (single-backend builds drop indirection via `if constexpr`); a *runtime-switchable* mode is opt-in via CMake for tools/editor scenarios that need it. Game shipping builds pin one backend.
- **Vulkan baseline = Timeline Semaphores (VK 1.2+).** No fallback path below this; `VkFence`-based sync is removed from the new layer.
- **Vulkan extensions are compile-time switches.** CMake options generate a `constexpr bool` capability header. Backend code uses `if constexpr` so disabled features cost zero binary size. Runtime device probing only *validates* the compile-time set — it does not branch into per-extension fallback ladders.
- **Devices use the highest level they support.** When an extension is compiled in, the backend always picks the strongest available path (e.g. prefer `VK_EXT_*_indirect_*` when present) and reports unsupported devices as unusable rather than silently degrading.
- **Shader Object vs Module is a build profile.** Engine/editor profile compiles `VK_EXT_shader_object` on for fast iteration; game/runtime profile compiles only `VkShaderModule` + PSO cache. Mixed mode is explicitly rejected (see deep-dive).
- **Resource handles are opaque indices.** Frontend never sees `VkImage`. Backends own native objects keyed by handle.
- **State tracking lives in the backend.** Frontend records *intent*; backend infers barriers, layout transitions, queue ownership.
- **Multi-threaded recording is first class.** Per-thread recorders, deterministic merge at submit. No locks on hot paths.
- **Validation is a separate decorator backend.** No `#ifdef DEBUG` validation interleaved with production code paths.

## Gap vs Code

| Area | Today | Target |
| --- | --- | --- |
| Layer split | Vulkan calls scattered through `RenderEngine/` | `libs/render/frontend/` + `libs/render/backend/vulkan/` + `libs/vk_core/` |
| API choice | Vulkan only, hard-wired | Compile-time backend pick; opt-in runtime switch |
| Vulkan baseline | Mixed sync primitives | Timeline Semaphore mandatory (VK 1.2+) |
| Extensions | Ad-hoc `vkGetDeviceProcAddr` checks | CMake → `constexpr` config; `if constexpr` in backend |
| Shader path | One code path | Compile-time profile: shader object *or* shader module |
| Recording | Direct `vkCmd*` on `vk::CommandBuffer` | Typed command stream, backend-agnostic verbs |
| Barriers | Manual / implicit | Backend-inferred from recorded resource access |
| Multi-thread | Single-threaded record | Per-thread recorders, deterministic merge |

## Deep-Dive Index

| File | One-line summary |
| --- | --- |
| [`render-layer-split.md`](./deep-dive/render-layer-split.md) | Three-layer split: `render/frontend` (CPU command stream) → `render/backend/<api>` (translation) → `<api>_core` (SDK substrate). |
| [`vulkan-extension-strategy.md`](./deep-dive/vulkan-extension-strategy.md) | Compile-time extension config, shader object/module profiles, Timeline baseline. |
| [`context-decomposition.md`](./deep-dive/context-decomposition.md) | Leaf classes hold handles only; `VulkanContext` splits into role-typed providers; init becomes free functions returning bundles. |
| [`swapchain-gui-decoupling.md`](./deep-dive/swapchain-gui-decoupling.md) | `SurfaceProvider` callback bundle; vk_core owns `vk::SurfaceKHR`; pull+push resize notification. |
| [`vk-core-non-blocking-contract.md`](./deep-dive/vk-core-non-blocking-contract.md) | vk_core never blocks; forbidden-API list; `RetireQueue` keyed by timeline value; module scope boundary. |

## Planned Deep-Dives (TODO)

- [ ] `command-recorder-design.md` — command stream encoding, arena allocation, per-thread recorders.
- [ ] `backend-translation-strategy.md` — recorded stream → native API; Vulkan first, DX12/Metal contract notes.
- [ ] `backend-selection-modes.md` — compile-time vs runtime backend switching; vtable-vs-`if constexpr` cost model.
- [ ] `resource-state-tracking.md` — automatic barrier inference, queue ownership transfers, alias analysis.
- [ ] `pso-and-shader-integration.md` — how `shader_system` packages bind to RHI PSO objects (cross-module contract goes through PRINCIPLES).
- [ ] `validation-decorator-backend.md` — separate validation backend, capture/replay support.

## Open Questions

- Should the frontend model render passes / dynamic rendering uniformly, or expose both as recorded verbs?
- Where does the line sit between "recorder" and "render graph"? Are they the same thing or stacked?
- For the runtime-switch backend mode: vtable dispatch on the command stream, or per-call-site type erasure?

## Changelog

- 2026-06-08 f1cd84f: 3-layer split (frontend / backend / api_core); add render-layer-split deep-dive
- 2026-06-08 f1cd84f: add context-decomposition, swapchain-gui-decoupling, vk-core-non-blocking-contract deep-dives
- 2026-05-29 04247ac: split into `libs/gfx/{frontend,backend/vulkan}`; pin Timeline baseline; extensions & shader path become compile-time profiles
- 2026-05-21 755fc72: initial overview, no deep-dives yet
