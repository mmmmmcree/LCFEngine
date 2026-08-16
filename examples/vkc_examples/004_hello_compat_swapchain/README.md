# 004 · hello_compat_swapchain

**Goal** — Declare the swapchain capability once and let the build-time probe
select `vkc::wsi::Swapchain` or its `vkc::wsi::compat::Swapchain` fallback for
the configured physical device.

- **Window backend:** GLFW. No special reason — like 003, the window library is
  only here to exercise WSI adaptation across GUI libraries (003 uses SDL3, 004
  uses GLFW).
- **Swapchain dependencies:** the probe prefers the maintenance1 path and falls
  back to the core compatibility path when its requirements are unavailable.
- **Other example deps (not the focus):** debug utils for validation logging.
- **What it draws:** identical to 003 (1×1 white image blitted to the swapchain).
- **Threading:** same cross-thread test as 003 — `present` on a dedicated
  render thread, GLFW events on the main thread.

**Compatibility fallback** — Without a present fence, there is no GPU signal
for when the presentation engine releases a present-ready semaphore. So:

- per-frame resources (cmd / acquire semaphore / fence) recycle **non-blocking**
  via the submit fence, exactly like 003;
- the present-ready semaphore is **fixed per image index** instead, reused only
  when that image is acquired again;
- **only `recreate` blocks** (`device.waitIdle`) — the unavoidable cost of
  dropping maintenance1. Steady-state present stays non-blocking.
