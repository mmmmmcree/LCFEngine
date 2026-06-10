---
parent: vk_core
title: swapchain-gui-decoupling
last-anchor-commit: aaaa573
---

# Swapchain ↔ GUI Decoupling

## Problem

`RenderEngine/include/Vulkan/VulkanSwapchain.h:80` holds a `gui::VulkanSurfaceBridgeSharedPointer` member, and `VulkanContext.cpp:32-34` reaches into `gui` to construct surface backends. This couples Vulkan's swapchain code to the windowing system module — a swapchain cannot be moved into `libs/vk_core/` without dragging `gui` along, and headless test builds cannot construct a swapchain at all. The bridge object owns and creates `VkSurfaceKHR` despite surface being a Vulkan handle whose destruction is `vkDestroySurfaceKHR`, an inversion of ownership.

## Design

### `vk_core` only needs two facts from GUI

A swapchain implementation needs the GUI module for exactly two operations: (1) creating `VkSurfaceKHR` from a platform-specific window handle, (2) querying the framebuffer extent on resize. Everything else (format/present-mode picking, image acquire, present, recycle) is pure Vulkan.

### `SurfaceProvider`: a callback bundle, not an interface

```cpp
namespace lcf::vkc {
struct SurfaceProvider {
    std::function<vk::SurfaceKHR(vk::Instance)> create_surface;
    std::function<vk::Extent2D()>               query_framebuffer_extent;
};
}
```

GUI implementations construct a `SurfaceProvider` capturing their native window handle and hand it to `vk_core`. No virtual interface, no inheritance — `std::function` is the boundary type. `vk_core` includes `<vulkan/vulkan.hpp>` and `<functional>`; it does **not** include any `gui/` header.

### `vk::SurfaceKHR` ownership lives in vk_core

Once `provider.create_surface` returns the raw handle, the swapchain wraps it in `vk::UniqueSurfaceKHR` and owns it for its lifetime. The GUI side retains only the window handle inside its lambda capture; it never holds the `VkSurfaceKHR`. This eliminates the current `bridge_sp->createBackend(this->getInstance())` inversion (`VulkanContext.cpp:33`).

### Resize notification: pull + push, both supported

- **Pull (mandatory fallback):** `tryAcquire` returns `OutOfDate`/`Suboptimal`; caller invokes `recreate()`, which calls `provider.query_framebuffer_extent()`. Always available, works on any driver.
- **Push (fast path):** swapchain exposes `markSurfaceDirty()`; GUI invokes it from window resize / minimize / restore events. Avoids one-frame stall waiting for the driver's `OUT_OF_DATE` return.

Both must coexist. Implicit recreate inside `acquire` is rejected — it silently swallows errors at an interrupt-driven point and complicates diagnostics.

### Non-blocking acquire/present surface

Aligned with the [`non-blocking-contract`](./non-blocking-contract.md): the swapchain exposes `tryAcquire(timeout = 0)` returning `expected<AcquiredImage, AcquireError>`, plus `present(...)`, `tickRetire(timeline_value)`, `markSurfaceDirty()`. No blocking acquire. `RenderEngine`'s frame loop, not the swapchain, decides what to do on `OutOfDate`.

### Headless mock falls out for free

A test or compute-only build provides a `SurfaceProvider` whose `create_surface` returns a null handle (or whose construction is skipped entirely if no swapchain is requested). The vk_core surface code requires no `#ifdef`s for headless support — the absence of a provider is the absence of a swapchain.

## Trade-offs

| Option | Pros | Cons | Verdict |
| --- | --- | --- | --- |
| Callback bundle (`SurfaceProvider`) | vk_core has zero `gui/` includes; headless trivial; GUI can be SDL/GLFW/Win32 transparently | Two `std::function` heap-allocs per swapchain (negligible) | ✅ chosen |
| Virtual `ISurfaceProvider` interface | Familiar OOP | Requires shared header; harder to keep vk_core dep-free | ❌ rejected |
| Keep `VulkanSurfaceBridge` as the boundary type | Minimal diff | The bridge would have to move into vk_core, dragging GUI knowledge with it | ❌ rejected |
| Implicit recreate inside `acquire` | One fewer call site | Hides errors, conflates control flow with side effects, breaks non-blocking contract | ❌ rejected |

## Landing Plan

- [ ] Phase 1 — Define `vk_core::SurfaceProvider` and `vkc::Swapchain` skeleton (handles, no GUI types).
- [ ] Phase 2 — Move the body of `VulkanSwapchain` into `vk_core/swapchain/`; replace `gui::VulkanSurfaceBridgeSharedPointer` member with `SurfaceProvider`. Move `vk::UniqueSurfaceKHR` ownership into the swapchain.
- [ ] Phase 3 — Convert API to non-blocking: `tryAcquire`, `present`, `tickRetire`, `markSurfaceDirty`. Remove `prepareForRender`/`finishRender` (current `VulkanSwapchain.h:28-29`) — fold their logic into RenderEngine's frame loop.
- [ ] Phase 4 — Provide `gui/sdl/makeSdlSurfaceProvider(SDL_Window *)` (and equivalents for SDL2/GLFW) returning a `SurfaceProvider`. Delete `gui::VulkanSurfaceBridge`.
- [ ] Phase 5 — Headless test exercising swapchain absence and a swiftshader-backed minimal `SurfaceProvider` mock.

## Changelog

- 2026-06-10 aaaa573: moved into vk_core module
- 2026-06-08 f1cd84f: created — SurfaceProvider callback bundle, surface ownership in vk_core, dual resize notification
