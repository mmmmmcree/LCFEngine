# LCFEngine Roadmap

The single authoritative source for **future-facing engine vision**. This is *not* a record of what the engine does today — that lives in source comments and `CLAUDE.md`.

## Who reads this

- **Humans** — start here, scan the module table below, open whichever `overview.md` you need.
- **LLMs / agents** — read `AGENTS.md` first, then follow the protocol there.

## Module Index

| Module | Scope | What it covers |
| --- | --- | --- |
| [`shader_system/`](./shader_system/overview.md) | vision | Slang language adoption, variant sparsity, reflection-driven editor, Editor/Runtime split, three-tier DDC, lighting model as replaceable interface |
| [`modern_rhi/`](./modern_rhi/overview.md) | vision | `libs/gfx/{frontend,backend/vulkan}` split; compile-time extension/shader-path profiles, opt-in runtime API switching, Vulkan Timeline baseline |
| [`ecs_async/`](./ecs_async/overview.md) | vision | EnTT + Taskflow + coroutines integration, async signal dispatch, system scheduling model |
| [`vulkan_optimizations/`](./vulkan_optimizations/overview.md) | short-term | Break SSBO single-copy GPU serialization, interface convergence, near-term Vulkan backend wins |

## Cross-cutting

- [`PRINCIPLES.md`](./PRINCIPLES.md) — design philosophy and constraints shared by every module.
- [`AGENTS.md`](./AGENTS.md) — protocol any LLM/agent must follow before editing this directory.
- [`_template/`](./_template/) — required skeletons for new `overview.md`, `deep-dive/*.md`, ADRs.
- [`_archive/`](./_archive/) — single-page ADRs for rejected proposals, kept only to prevent re-litigation.

## Conventions

- **One module = one directory.** Cross-module references are forbidden in module bodies; lift shared rules to `PRINCIPLES.md`.
- **Size caps.** `overview.md` ≤ 200 lines. Each `deep-dive/*.md` ≤ 300 lines. A module ≤ 6 deep-dives — beyond that, split the module.
- **Anti-bloat.** Frontmatter carries only `last-anchor-commit`. Outdated content is *deleted*, not annotated. Use `git log` for history.
- **No code dumps.** Code/pseudocode in deep-dives ≤ 15 lines per block. Anything larger belongs in source comments.

## Status

This directory is bootstrapping. Skeletons + first examples are landed. Most `deep-dive/` folders are empty by design — they fill up as decisions get made.
