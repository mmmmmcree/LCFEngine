# 003 · hello_swapchain

**Goal** — Present with the default `vkc::wsi::Swapchain`, which depends on the
`VK_KHR_swapchain_maintenance1` extension (present fence).

- **Window backend:** SDL3 (`sdl3[vulkan]`). No special reason — the window
  library is only here to exercise WSI adaptation across GUI libraries.
- **Swapchain dependencies:** extensions `VK_KHR_swapchain` +
  `VK_KHR_swapchain_maintenance1`, and the `synchronization2` feature (used by
  the barrier recording — a code choice, independent of maintenance1).
- **Other example deps (not the focus):** debug utils for validation logging.
- **What it draws:** a 1×1 white image blitted onto every swapchain image.
- **Threading:** the render loop runs on a **dedicated thread** (`present` in a
  loop) while the main thread pumps SDL events. This validates that the
  swapchain tolerates being driven cross-thread.

**Key point** — The present fence (from maintenance1) gives a per-present GPU
completion signal, so resources are recycled by polling that fence.
**Recycling never blocks**: `present` reclaims finished frames and returns
immediately, even on window resize.
