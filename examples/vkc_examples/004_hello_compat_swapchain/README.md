# 004 · hello_compat_swapchain

**Goal** — Present with `vkc::wsi::compat::Swapchain`, a fallback path that uses
**no extension beyond `VK_KHR_swapchain`** (no `synchronization2`, no
`swapchain_maintenance1`) for devices/drivers that lack them.

- **Window backend:** GLFW. No special reason — like 003, the window library is
  only here to exercise WSI adaptation across GUI libraries (003 uses SDL3, 004
  uses GLFW).
- **Swapchain dependencies:** extension `VK_KHR_swapchain` only — no
  `swapchain_maintenance1`, and no `synchronization2` feature (the barriers are
  recorded with core 1.0 calls).
- **Other example deps (not the focus):** debug utils for validation logging.
- **What it draws:** identical to 003 (1×1 white image blitted to the swapchain).
- **Threading:** same cross-thread test as 003 — `present` on a dedicated
  render thread, GLFW events on the main thread.

**Key point** — Without a present fence, there is no GPU signal for when the
presentation engine releases a present-ready semaphore. So:

- per-frame resources (cmd / acquire semaphore / fence) recycle **non-blocking**
  via the submit fence, exactly like 003;
- the present-ready semaphore is **fixed per image index** instead, reused only
  when that image is acquired again;
- **only `recreate` blocks** (`device.waitIdle`) — the unavoidable cost of
  dropping maintenance1. Steady-state present stays non-blocking.
