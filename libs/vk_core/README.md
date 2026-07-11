# vk_core

Design principles:

1. **Capabilities, not policies.** Expose what Vulkan can do in terms of
   Vulkan primitives (queue flags, extensions, features, surfaces). Do not
   bake in roles or usage patterns — those belong to the user.

2. **Evolve with Vulkan, without breaking anyone.** As Vulkan iterates,
   vk_core adds more efficient implementations (e.g. `wsi::Swapchain` vs
   `wsi::compat::Swapchain`) and richer features (e.g. static/dynamic
   pipelines, shader objects under `pipeline/`) as parallel variants:
   - **Optional**: users may ignore a variant entirely without affecting
     any other functionality; no virtual dispatch chooses for them.
   - **Non-intrusive**: new extensions plug in through registration
     mechanisms and their supporting infrastructure, so adding a
     capability never modifies existing implementations.

3. **Users declare intent, vk_core picks the mechanism.** Users state what
   they need (capabilities, threading, preferences); vk_core resolves how
   to provide it. Mechanisms never leak into the API surface, only into
   diagnostic logs.
