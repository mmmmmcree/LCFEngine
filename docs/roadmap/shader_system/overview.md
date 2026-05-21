---
module: shader_system
scope: vision
last-anchor-commit: 755fc72
---

# Shader System — Overview

> A compiler-grade, reflection-driven, editor/runtime-split shader pipeline.

## Current State

The engine compiles GLSL via a thin wrapper around shaderc and feeds SPIR-V into the Vulkan backend. Variants, reflection, and PSO selection are ad-hoc per material. There is no unified shader package format, no DDC, and editor-only shader work shares the same code path as runtime loading. Lighting code is hard-wired into a fixed forward+ path; swapping the lighting model means editing engine source.

## Target Design

- **Slang as the single authoring language**, with `interface` + generics replacing `#define` permutation hell.
- **Variants are explicit, sparse, and authored** — never combinatorial; every variant is a named entry point.
- **Reflection is the contract** between shaders and engine: bindings, uniforms, vertex layouts, constants — all derived, never hand-mirrored in C++.
- **Editor and Runtime are two libraries, two binaries.** `ShaderCore.Editor` does compilation, reflection, baking; `ShaderCore.Runtime` only loads and binds prebaked packages.
- **Three-tier DDC** — local memory cache → on-disk per-user cache → shared team cache. PSO cache prewarms from DDC at startup.
- **Lighting model is a swappable `interface ILightingModel`**, injected as a generic parameter into the shading pass; engine does not own a fixed BRDF.
- **Shader package = self-describing bundle** of SPIR-V + reflection + variant index, addressed by content hash, signed.
- **Hot-reload is editor-only** and goes through a dedicated channel, not the runtime loader.

## Gap vs Code

| Area | Today | Target |
| --- | --- | --- |
| Authoring | GLSL + shaderc | Slang + slangc |
| Variants | `#define` recompiles | Named entry points + sparse index |
| Reflection | Manual C++ structs mirror layouts | Auto-generated from compiler IR |
| Editor/Runtime | One library, branched by `#if EDITOR` | Two libraries, runtime cannot link compiler |
| Caching | None | Three-tier DDC + PSO prewarm |
| Lighting | Forward+ baked into engine | `ILightingModel` plug-in via generics |

## Deep-Dive Index

| File | One-line summary |
| --- | --- |
| `deep-dive/lighting-model-as-interface.md` | Slang `interface ILightingModel` + generic shading pass to replace hard-wired BRDF. |
| `deep-dive/editor-runtime-separation.md` | Two-library split, package format, three-tier DDC, PSO prewarm. |

## Planned Deep-Dives (TODO)

- [ ] `slang-adoption-strategy.md` — migration plan from GLSL/shaderc to Slang/slangc, build integration.
- [ ] `variant-sparse-index.md` — naming scheme, dependency graph, dead-variant culling.
- [ ] `reflection-driven-bindings.md` — IR → C++ binding tables, validation at load.
- [ ] `pso-cache-and-prewarm.md` — startup prewarm strategy, eviction, telemetry.

## Open Questions

- Is Slang's runtime reflection format stable enough to ship as part of our package, or do we ship our own thinner schema?
- Where does material parameter authoring live — inside the shader package, or in a sibling material asset?
- How do we represent platform-specific variants (mobile / desktop / next-gen) without exploding the index?

## Changelog

- 2026-05-21 755fc72: initial overview + first two deep-dives landed
