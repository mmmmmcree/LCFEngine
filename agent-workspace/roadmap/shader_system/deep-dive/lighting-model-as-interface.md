---
parent: shader_system
title: lighting-model-as-interface
last-anchor-commit: 755fc72
---

# Lighting Model as a Replaceable Interface

## Problem

In legacy engines the lighting model is welded into the engine's shading code: the BRDF, the light loop, the shadow sampling, the GI integration — all share one fixed function call graph. Replacing the BRDF or experimenting with a new energy-conserving sheen model requires editing engine source and recompiling. This is the largest contributor to "you cannot use Engine X for stylized rendering without forking it."

LCFEngine must avoid this. The shading pass should not know which BRDF it is evaluating — only that *some* lighting model implements an agreed contract.

## Design

- Slang exposes an `interface` declaring the lighting contract. The shading pass takes the implementation as a generic type parameter.
- Materials choose their `ILightingModel` implementation at authoring time; the variant index records the chosen type.
- Engine ships a small set of stock implementations (Standard PBR, Cloth, Hair) but does **not** privilege any of them in the pass code.
- New lighting models are pure additive content — no engine recompile, no `#ifdef NEW_MODEL` proliferation.

```slang
interface ILightingModel {
    float3 evaluate(ShadingContext ctx, LightSample light);
    float3 indirect(ShadingContext ctx, ProbeSample probe);
};

void shade<T : ILightingModel>(ShadingContext ctx, T model) {
    float3 lit = 0;
    for (LightSample s in iterateLights(ctx)) lit += model.evaluate(ctx, s);
    lit += model.indirect(ctx, sampleProbe(ctx));
    writeOutput(lit);
}
```

`ShadingContext`, `LightSample`, `ProbeSample` are engine-defined POD-like structs whose layout is shared with all implementations and tracked by reflection.

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Slang `interface` + generic | Static dispatch, zero runtime cost, type-checked contract | Requires Slang adoption; slangc maturity risk | ✅ chosen |
| `#define LIGHTING_MODEL_X` macro | Works in plain GLSL/HLSL today | Fragile, no contract checking, combinatorial explosion | ❌ rejected |
| Function pointer table at runtime | Maximum flexibility | Indirect call cost, breaks inlining, defeats GPU divergence assumptions | ❌ rejected |
| Hardcoded BRDF, expose only parameters | Simple, fast | Locks us out of stylized / non-physical models | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — define `ShadingContext` / `LightSample` / `ProbeSample` POD layouts and freeze them via reflection tests.
- [ ] Phase 2 — implement Standard PBR as the first `ILightingModel`; rewrite the existing forward+ pass to consume it generically.
- [ ] Phase 3 — port shadow + GI sampling to be context-driven; verify no engine code references a specific BRDF.
- [ ] Phase 4 — author Cloth and Toon implementations as pure content to validate the contract.
- [ ] Phase 5 — gate engine review on "no `BRDF_*` symbol appears outside an `ILightingModel` impl".

## Changelog

- 2026-05-21 755fc72: created
