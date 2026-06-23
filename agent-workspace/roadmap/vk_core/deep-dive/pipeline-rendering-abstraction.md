---
parent: vk_core
title: pipeline-rendering-abstraction
last-anchor-commit: a856fe6
---

# Pipeline / Rendering Abstraction

## Problem

`libs/vk_core/include/vk_core/pipeline/Pipeline.h` is an empty placeholder — vkc
has no pipeline abstraction yet. The `pipeline/` module splits by execution type:
`graphics/`, `compute/`, `raytracing/`. Within `graphics/`, two orthogonal axes
must be expressed at the SDK-substrate level: how shaders bind (baked
`VkPipeline` vs `VK_EXT_shader_object` virtual pipeline) and how a render scope
opens (`VkRenderPass` + framebuffer vs `vkCmdBeginRendering`). Render scope is a
graphics-only concept — `compute`/`raytracing` have no rendering axis and no
static/dynamic split. Without a unifying shape the graphics axes leak into every
recording site as `if (shaderObject) …` branches tangled with render-scope
setup. vkc should expose **both implementations behind one recording
interface**; which one a build uses is a render-layer profile choice (see
modern_rhi `vulkan-extension-strategy`), not something vkc decides.

## Design

Four classes on two axes; **three legal pairings**, one excluded:

|            | Rendering                                  | Pipeline                                       |
| ---        | ---                                        | ---                                            |
| **Static** | `StaticRendering` (VkRenderPass + FBO)     | `StaticPipeline` (baked `VkPipeline`, PSO)     |
| **Dynamic**| `DynamicRendering` (`vkCmdBeginRendering`) | `DynamicPipeline` (shader-object virtual PSO)  |

- `StaticPipeline` pairs with **either** rendering — a baked PSO is valid under a
  render pass instance and under dynamic rendering.
- `DynamicPipeline` pairs with **`DynamicRendering` only**. Deliberate scope cut,
  not a Vulkan limit (shader objects can legally run inside a render pass) — vkc
  declines to support the combo to avoid a full 2×2 matrix. Excluded:
  `DynamicPipeline + StaticRendering`.

**One metadata Info drives both paths.** A single descriptor (e.g.
`RenderingInfo`) is the source of truth, consumed two ways:

- `StaticRendering` reads it **offline** to build `VkRenderPass` / framebuffer
  objects once.
- `DynamicRendering` reads it **at record time** to assemble the
  `vkCmdBeginRendering` begin info.

The pipeline side mirrors this: one pipeline-state Info either bakes a PSO
(static) or seeds the `vkCmdSet*` dynamic-state calls (dynamic).

**Unified `begin(cmd)` interface.** The caller passes the command buffer; each
class assembles its own `vkCmd*` internally, so the static/dynamic seam is
invisible above the abstraction. vkc stays frame-agnostic — no ring/profile
logic here.

```cpp
// caller is path-agnostic
rendering.begin(cmd);   // -> beginRenderPass | beginRendering
pipeline.bind(cmd);     // -> bindPipeline    | bindShadersEXT + setViewport...
cmd.draw(...);
rendering.end(cmd);     // -> endRenderPass   | endRendering
```

**Single module, no cross-feature dependency.** All four classes live in one
module (`pipeline/graphics/`), so `RenderingInfo` is shared *inside* the module —
it is not a separate feature module that `*Pipeline` would have to depend on.
The only Pipeline↔Rendering coupling (a static PSO must know its render-pass
handle, or its attachment formats under dynamic rendering) is satisfied with
**vk-native values passed at the call site** (`vk::RenderPass` / a
`std::span<const vk::Format>`), never by one class holding the other. This keeps
the module's external dependency surface limited to tools/substrate
(`manifest`, `context`, `sync`, `error`) — matching how `WSI/Swapchain` depends
only on substrate, never on sibling feature modules.

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| One Info → both paths; `begin(cmd)` self-assembles | Caller path-agnostic; static/dynamic seam erased; metadata authored once | Info must be a superset both paths accept; class internals diverge | ✅ chosen |
| Expose render pass and dynamic rendering as separate caller-visible verbs | No hidden translation | Branchy record sites; profile leaks into callers; defeats the abstraction | ❌ rejected |
| Allow full 2×2 (incl. `DynamicPipeline + StaticRendering`) | Maximum flexibility | Doubles tested combos for a path no profile ships | ❌ rejected (scope cut) |
| Shader-object only, drop static classes | Simplest | No game/legacy profile possible upstairs; loses PSO-cache determinism | ❌ rejected |
| One module shares `RenderingInfo` | Zero cross-feature dependency; coupling stays vk-native values at call site | Pipeline + rendering co-located despite being distinct concepts | ✅ chosen |
| Separate pipeline/ + rendering/ modules, pipeline depends on rendering | Concept-pure separation | Violates vkc's feature-modules-never-depend-on-each-other rule | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — Lay out `pipeline/{graphics,compute,raytracing}`; in `graphics/` define the shared Info structs (`RenderingInfo`, pipeline-state Info) as the single source of truth. Retire the `Pipeline.h` stub.
- [ ] Phase 2 — Implement `graphics/StaticRendering` + `StaticPipeline`; coupling via call-site vk-native values. Validate with a static fullscreen-triangle example to swapchain (slang literal verts, no buffer/image/descriptor).
- [ ] Phase 3 — Implement `graphics/DynamicRendering` + `DynamicPipeline` (shader object); confirm device-level dispatch resolves `vkCmd*EXT` (see `api_dispatch.cpp` `LCF_VKC_USE_DEVICE_DISPATCH`). Two more examples: static+dynamic, dynamic+dynamic — identical output.
- [ ] Phase 4 — Lock `begin(cmd)`/`bind(cmd)`; enforce `DynamicPipeline → DynamicRendering` at the type level. `register_dynamic_rendering` / `register_shader_object` as the module's two extension entries.

## Changelog

- 2026-06-23 a856fe6: created — 4-class two-axis model, 3 legal pairings, one-Info-drives-both, unified begin(cmd)
