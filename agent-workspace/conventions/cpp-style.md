# C++ Style — LCFEngine

Living document. Updated by the agent on every style correction from the human (see [`AGENTS.md`](./AGENTS.md)). Baseline naming (PascalCase classes, `m_snake_case` members) is in `CLAUDE.md`; this file records refinements and corrections confirmed by the human.

## Naming

- **Class methods**: camelCase (per CLAUDE.md). **Free functions**: `snake_case` — including helpers in anonymous namespaces and `init`-style module functions (`prefer_most_queue_parallelism`, `device_extension_union`).
- **constexpr / file-scope constants**: `k_snake_case` (`k_sync`, `k_max_variable_descriptor_count`).
- **No abbreviated names** for folders or types. **Namespaces are the exception**: short forms the human has established are preferred (`vkc`, `conf`, `bs`, `surf`); do not invent new abbreviations without confirmation.
- **Avoid C++ keywords as identifiers** even where context-sensitive (`module`); prefer encoding identity in the variable name over a name/tag field.
- **No stutter with the enclosing namespace**: inside `namespace manifest` the type is `Entry`, not `ManifestEntry`.
- Pointer-typed locals/members carry `_p` suffix (`entry_p`, `m_head_p`); optionals carry `_opt`.

## File layout (.cpp)

Top-down readability: the design first, the plumbing last.

1. Includes.
2. File-scope **anonymous namespace** containing helper *declarations* only. Never nest the anonymous namespace inside the project's named namespace.
3. The named namespace (`lcf::vkc::init`, …) with the **core interface definitions** — this is what a reader opens the file for.
4. A second file-scope anonymous namespace block at the bottom with helper *definitions*.

## Classes

- Singletons: class-static `instance()` method with private constructor — not a free accessor function.
- `using Self = ClassName;` idiom for the rule-of-five declarations.
- Value types intended to cross API boundaries: movable, copy deleted unless copying is meaningful.

## Comments

- Code explains itself; if it needs a comment to be understood, redesign it.
- Comments are acceptable for exactly two things: placeholders standing in for elided implementation in skeletons, and **non-obvious constraints the code cannot express** (e.g. "must stay non-constexpr: registration relies on dynamic initialization").
- Never: comments narrating what the next line does, where code came from, or why a change is correct.

## Unsettled

- CLAUDE.md says constants are SCREAMING_SNAKE_CASE, but the codebase and confirmed new code use `k_snake_case` — CLAUDE.md likely needs updating; ask before touching constants in old modules.
