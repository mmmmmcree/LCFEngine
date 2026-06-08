---
parent: shader_system
title: editor-runtime-separation
last-anchor-commit: 755fc72
---

# Editor / Runtime Separation and the Shader Package Pipeline

## Problem

Conflating editor and runtime shader code is the single largest source of bloat in legacy engines: the runtime drags in slangc/dxc/glslang, the editor's hot-reload watcher leaks into shipped builds, debug-only reflection metadata ships to players, and shader compile times tax every game launch. Cold-start PSO compilation hitches are the visible symptom; the root cause is the absence of a *package* format and a *split*.

LCFEngine treats Editor and Runtime as two binaries with no shared symbols beyond plain data structures.

## Design

- **Two libraries.** `ShaderCore.Editor` links the Slang compiler, reflection emitter, and DDC writer. `ShaderCore.Runtime` links neither — it can only *consume* prebaked packages.
- **Shader Package.** Self-describing bundle: `{ header, variant index, SPIR-V blobs, reflection blob, signature }`. Content-hash addressed.
- **Three-tier DDC.**
  1. In-memory LRU per editor session.
  2. On-disk per-user cache under the project's intermediate directory.
  3. Shared team cache (read-only for clients, write via CI).
- **PSO prewarm.** At runtime startup, the package's variant index is replayed into the RHI's PSO cache before the first frame; missing PSOs trigger background compile, never blocking submission.
- **Hot-reload** is an editor-only channel: file watcher → recompile via Editor lib → publish new package over IPC to a connected runtime instance. Shipping builds neither watch files nor parse Slang source.

```text
authoring          (Editor only)               (Runtime only)
*.slang  ──►  ShaderCore.Editor  ──►  *.shdpkg  ──►  ShaderCore.Runtime  ──►  RHI PSO cache
                  │   │
                  │   └─ writes DDC tier-2 / tier-3
                  └─ reads DDC for cache hits
```

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Two libraries + package format | Clean shipping size, fast launches, no compiler in player builds | Higher upfront design cost, two build targets | ✅ chosen |
| Single library, `#if EDITOR` branches | Familiar, minimal upfront cost | Editor symbols leak via inlining, RTTI, debug info | ❌ rejected |
| Runtime-only with on-demand compile | Simplest mental model | Ships compiler, ships source, slow cold start | ❌ rejected |
| Editor-only, ship raw SPIR-V loose files | No package complexity | No content-hash addressing, no DDC, no signature | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — define `.shdpkg` v0 schema and content-hash addressing scheme.
- [ ] Phase 2 — split current shader code into `ShaderCore.Editor` and `ShaderCore.Runtime` targets; runtime must not link slangc.
- [ ] Phase 3 — implement DDC tier-1 and tier-2; CI populates tier-3.
- [ ] Phase 4 — RHI PSO cache prewarm path; telemetry for cache miss rate at first-frame.
- [ ] Phase 5 — editor IPC hot-reload channel; verify shipping build has no file watcher.

## Changelog

- 2026-05-21 755fc72: created
