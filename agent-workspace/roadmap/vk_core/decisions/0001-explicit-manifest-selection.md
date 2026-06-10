---
adr: 0001
parent: vk_core
status: accepted
date: 2026-06-10
last-anchor-commit: aaaa573
---

# ADR-0001: Explicit manifest selection replaces include-driven registration

## Context

The first manifest design auto-registered a module's dependency entry via an inline variable in its headers — "include = depend". Review surfaced three problems: the registry is process-global, so two bootstraps in one process share a union and need a subtraction mask (which demands global include knowledge); the mechanism rests on linkage subtleties (COMDAT merging, non-foldable dynamic init, umbrella-header discipline); and registration crept into class headers like `TimelineSemaphore.h`, which should not know about bootstrap at all.

## Decision

Each module ships `<module>/feature_dependencies.h` exporting a constexpr `mnf::FeatureDependencies` (e.g. `mnf::k_sync`) covering everything the module's classes unconditionally require, with nice-to-haves as `optional` children. Callers pass exactly the manifests they use to `init::bootstrap` — the dependency entry point mirrors `cmake/scripts/deps.cmake`.

## Consequences

- (+) Compile-time auditable at the bootstrap call site; per-bootstrap granularity, multi-instance divergence is natural.
- (+) No linkage magic, no umbrella-header discipline, no registry singleton.
- (+) Class headers stay bootstrap-agnostic; dependency knowledge lives in one file per module.
- (−) A caller can forget a manifest; caught at module `create` entry points via capability-snapshot check returning an error, not at compile time.
- (−) The manifest list duplicates usage knowledge by hand.

## Alternatives Considered

- **Include-driven inline registration** — rejected for the three context problems above.
- **Static registration in module .cpp** — rejected: silently dead-stripped by static-library linking.
- **`Cap<X>` permission tokens on every class constructor** — rejected: same audit benefit as the explicit list with far more ceremony.
