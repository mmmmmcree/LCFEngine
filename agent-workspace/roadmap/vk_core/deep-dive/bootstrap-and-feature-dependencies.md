---
parent: vk_core
title: bootstrap-and-feature-dependencies
last-anchor-commit: aaaa573
---

# Bootstrap & Feature Dependencies

## Problem

A Vulkan extension touches code at five fixed points: instance extension list, device extension list, feature query (pNext chain on `getPhysicalDeviceFeatures2`), feature enable (pNext chain at device create), and new entry points (dispatch). All five happen at bootstrap; runtime only *uses* the capability. Without a per-module dependency entry point, every new extension is a bootstrap edit and dependency knowledge scatters across class headers. The target division of labor: each module declares its dependencies in one place; the bootstrap caller selects which modules it uses; bootstrap folds the selection with zero per-extension knowledge — vkc provides capability, never guarantees device support.

## Design

### `conf::FeatureDependency`: one dependency entry point per module

Declaration types live in `config/FeatureDeclare.h` under `lcf::vkc::conf`. Each module ships `<module>/feature_dependencies.h` — the analog of `cmake/scripts/deps.cmake` — exporting, in its own namespace, `vkc::<module>::k_module_dependency`: everything the module's classes **unconditionally require** (using `sync` means using `TimelineSemaphore`, so its feature belongs to the module itself). Nice-to-have capabilities are separate `k_xxx_dependency` objects hung off `optional`; they never gate the module. Identity is the variable name — no name/tag field (and `module` is a C++ keyword).

```cpp
struct conf::FeatureDependency {
    uint32_t core_since = 0;
    std::span<const char * const> instance_extensions = {};
    std::span<const char * const> device_extensions = {};
    std::span<const FeatureBit> features = {};
    std::span<const FeatureDependency * const> optional = {};
};
```

A feature is declared as a single pointer-to-member NTTP — `conf::k_feature<&vk::PhysicalDeviceVulkan12Features::timelineSemaphore>` means "this member must be true"; the variable template derives both the enable side (set in the request chain) and the test side (read from the queried chain), so modules never write per-feature lambdas.

All content is constexpr. `core_since` drops the extension names when the chosen API version includes the capability as core. `conf::FeatureChain` is the runtime counterpart of `vk::StructureChain` — the struct set depends on the caller's selection, which a compile-time-fixed chain cannot host. Exemplars: `sync/feature_dependencies.h` (core feature, no extensions) and `surface/feature_dependencies.h` (`VK_KHR_surface` + `VK_KHR_swapchain`, optional `k_swapchain_maintenance_dependency`; platform window-system instance extensions come from the GUI side with `SurfaceProvider`, not from the manifest).

### Explicit selection at bootstrap

The caller passes exactly the modules it uses; class headers know nothing about bootstrap:

```cpp
auto context = bs::bootstrap({
    .app_name = "editor",
    .feature_dependencies = {&sync::k_module_dependency, &surf::k_module_dependency},
});
```

The call site is the compile-time audit surface for "what does this binary want". Two bootstraps in one process (editor window + headless baker) simply pass different lists. Include-driven auto-registration was tried and superseded — see `decisions/0001`.

### Wish-list fold semantics

| Role | Question answered | Knowledge source |
| --- | --- | --- |
| `config.feature_dependencies` | what this bootstrap *wants* | the caller — it knows what it is about to use |
| Device query → capability snapshot | what this bootstrap *got* | per-dependency intersection at fold time |
| Routing on the snapshot | what actually runs | render layer (compile-time profile or runtime branch) |

`bs::bootstrap(config)`, in order: flatten (expand `optional`, dedup) → dump to log → create instance with wish ∩ available instance extensions → per physical device evaluate each dependency's extensions + `is_supported` → score devices (most supported dependencies, then queue-family parallelism) → create device enabling only supported dependencies' extensions and features → freeze the capability snapshot into the `DeviceContext`, populate its `QueueContext`s (every queue of every family) and build the `QueueRole` index via the collapse ladder (dedicated family → spare graphics-family queue → eMainGraphics; aliased roles are safe because `QueueContext` is the single funnel) → emplace it into the returned `Context` (sorted by score) and build the `DeviceRole` index (eMain = strongest; eCompute aliases eMain on single-GPU machines). Ownership is strictly nested: a `Context` owns n `DeviceContext`s, each owns its `QueueContext`s; bootstrap creates one device context for the chosen device, more can be added for multi-GPU. Role tables are `std::array` sized by `lcf::enum_count_v` — no `eCount` sentinels.

No device is rejected by vkc. The snapshot is the runtime contract: module `create` entry points (`Swapchain::create`, …) check it and return an `error_code` on absence — a forgotten dependency surfaces there with a local, named cause. Hard "no device, abort" policy, like all policy, belongs upstairs.

### Accepted costs and mitigations

| Cost | Mitigation |
| --- | --- |
| Dependency list is hand-maintained; a caller can forget one | snapshot check at every module `create` entry point turns omission into a local, named error |
| Unused `optional` capabilities may still be enabled | scorer prefers them only as tie-breakers; shipping builds pass a narrower list |

### Namespace roles

Folders mirror Vulkan functional domains (for maintainers); namespaces mirror caller roles:

| Namespace | Role | Contents |
| --- | --- | --- |
| `lcf::vkc` | per-frame vocabulary | all daily-use types, flat |
| `lcf::vkc::bs` | one-shot vocabulary | bootstrap free functions, configs, device scorer |
| `lcf::vkc::conf` | declaration types | `FeatureDependency`, `FeatureChain` |
| `lcf::vkc::<module>` | per-module declarations | `k_module_dependency`, optional `k_xxx_dependency` |
| `lcf::vkc::enums` | vkc-invented enums only | result/strategy enums; vk-native enums are never mirrored |
| `lcf::vkc::details` | do not touch | `Memory<T>` and other internals |

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Per-module `feature_dependencies.h` + explicit list at bootstrap | compile-time audit at call site; per-bootstrap granularity; class headers bootstrap-agnostic; deps.cmake-shaped mental model | forgetting a dependency is a runtime (not compile) error; list duplicates usage knowledge | ✅ chosen |
| Include-driven inline registration in module headers | impossible to forget | process-global union; linkage subtleties; registration leaks into class headers | ❌ superseded (decisions/0001) |
| Static registration in module .cpp | conventional | dead-stripped by static-lib linking, silently | ❌ rejected |
| Central hand-maintained enable list inside init | simple | every extension is a bootstrap edit; modules lose ownership of their deps | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — `config/FeatureDeclare.h`: `FeatureDependency`, `FeatureChain`.
- [ ] Phase 2 — `sync/feature_dependencies.h` (core-feature exemplar) + `surface/feature_dependencies.h` (extension exemplar with `optional`).
- [ ] Phase 3 — `bs::bootstrap`: flatten + wish-list fold, scorer, capability snapshot, full-topology queue creation, returned `Context` owning the `DeviceContext`(s).
- [ ] Phase 4 — dependency/snapshot logging; snapshot checks in module `create` entry points; remaining modules (`memory`, `descriptor`, `shader`) get manifests.
- [ ] Phase 5 — CI builds one dependency-poor (headless) and one dependency-rich (editor) binary; assert their snapshots differ as expected.

## Changelog

- 2026-06-10 aaaa573: DeviceRole index on Context; enum_count_v sizing replaces eCount sentinels
- 2026-06-10 aaaa573: k_feature<&Struct::member> NTTP replaces per-dependency request/test lambdas
- 2026-06-10 aaaa573: QueueRole collapse ladder; ownership(vector)/index(array-by-enum) split
- 2026-06-10 aaaa573: bootstrap returns Context owning DeviceContexts owning QueueContexts; init→bs
- 2026-06-10 aaaa573: explicit per-module feature_dependencies replaces include-driven registry (decisions/0001)
