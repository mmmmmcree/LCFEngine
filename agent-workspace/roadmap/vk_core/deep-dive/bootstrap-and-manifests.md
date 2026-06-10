---
parent: vk_core
title: bootstrap-and-manifests
last-anchor-commit: aaaa573
---

# Bootstrap & Manifest Registry

## Problem

A Vulkan extension touches code at five fixed points: instance extension list, device extension list, feature query (pNext chain on `getPhysicalDeviceFeatures2`), feature enable (pNext chain at device create), and new entry points (dispatch). All five happen at bootstrap; runtime only *uses* the capability. A central hand-maintained enable list makes every new extension a bootstrap edit, and lets "what this binary requires" drift from "what the code actually uses". The target division of labor: a slice provides capability unconditionally; *using* a slice is what creates the dependency; bootstrap folds whatever is used with zero per-extension knowledge.

## Design

### ManifestEntry: constexpr data per slice

Each functional submodule ships `<slice>/manifest.h` declaring one entry per switchable capability:

```cpp
struct manifest::Entry {
    std::string_view slice;
    uint32_t core_since = 0;
    std::span<const char * const> instance_extensions = {};
    std::span<const char * const> device_extensions = {};
    void (*request_features)(FeatureChain &) = nullptr;
    bool (*is_supported)(const FeatureChain & queried) = nullptr;
};
```

All content is constexpr; the only runtime act is registration. `core_since` drops the extension names when the chosen API version already includes the capability as core. `FeatureChain` is the runtime counterpart of `vk::StructureChain`: the struct set is determined at link time by registration, which a compile-time-fixed chain cannot host.

### Include-driven registration

A slice's user-facing headers include their own `manifest.h`, which defines an `inline` variable whose initializer appends the entry to the central `manifest::Registry::instance()` (function-local static). Load-bearing linkage facts:

- The registration variable is instantiated in **consumer TUs** (whoever includes the header), which are always linked — immune to the static-library dead-stripping that kills registration placed in the slice's own `.cpp`.
- `inline` + COMDAT merging → one instance, one initialization, regardless of how many TUs include the header.
- The registry accessor is a function-local static — no static-init-order fiasco.
- `Registry::add` must be a genuine runtime side effect (non-constexpr), or the initializer is constant-folded and registration silently vanishes. Hard constraint, documented at the definition.

A slice never included anywhere contributes nothing. Include = depend.

### Wish-list fold semantics

The registry is a **wish list, not a demand list**. Three roles answer three questions:

| Role | Question answered | Knowledge source |
| --- | --- | --- |
| Registry (include-driven, global) | what this binary *could* want | slice self-registration |
| Device query → capability snapshot | what this bootstrap *got* | per-entry intersection at fold time |
| `BootstrapConfig.required` (positive, per-bootstrap) | what this call site *cannot live without* | the caller — local knowledge only |

`init::bootstrap(config)`, in order: dump registry to log → create instance with wish ∩ available instance extensions → per physical device: `getFeatures2` into a chain built from all `request_features`, evaluate each entry's `is_supported` → **reject only devices missing a `required` entry** → score survivors (default: most supported entries, then queue-family parallelism) → create device enabling only supported entries' extensions and features → freeze the result as an immutable capability snapshot in `DeviceContext` → create **every queue of every family** into the `QueueTable`.

The snapshot is the runtime contract for "can I use this": slice creation entry points (`Swapchain::create`, …) check it and return an `error_code` on absence — failure surfaces at the use site, not as a mysterious device rejection. `required` references the same `manifest::k_*` objects used for registration; requiring is legitimate because the caller necessarily knows what it is about to use, whereas masking would demand knowledge of what *other* code's includes dragged in.

Two bootstraps in one process (editor window + headless baker) read the same registry and diverge only in `required` — the union is harmless because unsupported, unused entries no longer reject anything.

### Accepted costs and mitigations

| Cost | Mitigation |
| --- | --- |
| Detection unit is the include graph, not actual calls — a stray include over-enables | Harmless by default (unused enabled extensions); **no umbrella header** keeps it rare |
| Some extensions cost driver overhead when enabled unused | optional `enable_policy` narrows enabling to `required` for shipping builds |
| Compile-time auditability of an explicit fold list is lost | Bootstrap logs registry + snapshot on every init |
| Constant folding can elide registration | `add()` non-constexpr; constraint pinned in `manifest/registry.h` |

### Namespace roles

Folders mirror Vulkan functional domains (for maintainers); namespaces mirror caller roles:

| Namespace | Role | Contents |
| --- | --- | --- |
| `lcf::vkc` | per-frame vocabulary | all daily-use types, flat |
| `lcf::vkc::init` | one-shot vocabulary | bootstrap free functions, configs, device scorer |
| `lcf::vkc::manifest` | declaration vocabulary | `Entry`, `FeatureChain`, `Registry`, per-slice `k_*` entries |
| `lcf::vkc::enums` | vkc-invented enums only | result/strategy enums; vk-native enums are never mirrored |
| `lcf::vkc::details` | do not touch | `Memory<T>` and other internals |

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Include-driven registry + wish-list fold + positive `required` | usage automatically implies declaration; new slices need zero bootstrap edits; multi-bootstrap divergence via local knowledge only | include hygiene discipline; runtime-only audit | ✅ chosen |
| Hard fold (any unsupported registered entry rejects the device) | strict | stray include rejects devices mysteriously; multi-bootstrap needs a subtraction mask, which demands global include knowledge | ❌ rejected |
| Explicit fold list + `Cap<X>` tokens at the bootstrap call site | compile-time auditable; per-callsite granularity | dependency list hand-maintained, duplicated against actual usage | ❌ rejected |
| Static registration in slice `.cpp` | conventional | dead-stripped by static-lib linking, silently | ❌ rejected |
| Central hand-maintained enable list in init | simple | every extension is a bootstrap edit; drifts from usage | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — `manifest/registry.h`: `Entry`, `FeatureChain`, `Registry::instance()`; the non-constexpr `add` constraint pinned.
- [ ] Phase 2 — `sync/manifest.h` (core-feature exemplar) + `shader/manifest.h` for shader object (optional-extension exemplar); slice headers include their manifests.
- [ ] Phase 3 — `init::bootstrap`: wish-list fold, `required` filter + scorer, capability snapshot, full-topology `QueueTable`.
- [ ] Phase 4 — registry/snapshot logging; `enable_policy`; remaining slices (`surface`, `memory`, `descriptor`) get manifests; snapshot checks in slice `create` entry points.
- [ ] Phase 5 — CI builds one slice-poor (headless) and one slice-rich (editor) binary; assert their logged manifests differ as expected.

## Changelog

- 2026-06-10 aaaa573: wish-list fold semantics; positive `required` replaces subtraction mask
- 2026-06-10 aaaa573: created — include-driven manifest registry, bootstrap fold, namespace roles
