# vkc Context Initialization — Design Notes

Scratch doc for you to implement against. Describes the full flow from `ContextCreateInfo` to a ready `Context` (instance → device → queues), plus vkc-owned surface creation. Not governed by any AGENTS protocol — keep or delete after use.

Anchors current code at: `RenderEngine/src/Vulkan/VulkanContext.cpp` (the shape we are replacing), `RenderEngine/include/Vulkan/VulkanFramebufferObject.h` (the CreateInfo style we follow).

---

## 1. `ContextCreateInfo` — the single creation entry point

Carries every parameter from instance down to queues. Follows the project CreateInfo idiom (`VulkanFramebufferObjectCreateInfo`): `using Self`, defaulted ctor, fluent `setXxx`/`addXxx` returning `Self &`, `getXxx` accessors, public `m_` fields under a `//- properties` marker. It is a superset of `vk::InstanceCreateInfo` data, not a wrapper of it.

```cpp
namespace lcf::vkc {
struct ContextCreateInfo {
    using Self = ContextCreateInfo;
    using FeatureDependencyList = std::vector<const conf::FeatureDependency *>;

    ContextCreateInfo(std::string_view app_name = "LCFEngine",
                      uint32_t api_version = vk::ApiVersion13) : ...

    Self & setAppName(std::string_view);
    Self & setApiVersion(uint32_t);
    Self & addInstanceExtension(const char *);     // explicit extras (debug utils, ...)
    Self & addInstanceLayer(const char *);         // validation, ...
    Self & addFeatureDependency(const conf::FeatureDependency *);  // surf::k_module_dependency, ...
    Self & setDeviceScorer(std::function<int(vk::PhysicalDevice)>);
    Self & enablePresentation(bool);               // see §3: gates surface instance extensions

    // getXxx accessors ...

//- properties
    std::string_view m_app_name;
    uint32_t m_api_version;
    std::vector<const char *> m_instance_extensions;
    std::vector<const char *> m_instance_layers;
    FeatureDependencyList m_feature_dependencies;
    std::function<int(vk::PhysicalDevice)> m_device_scorer;
    bool m_enable_presentation = false;
};
}
```

Entry point: `vkc::createContext(const ContextCreateInfo &) -> std::expected<Context, std::error_code>`. The `bs::bootstrap` name and namespace retire; suggest `lcf::vkc::createContext` free function (or `Context::create`).

### Extension/layer assembly (three merged sources)

At instance creation the enabled extension set is the **union** of:
1. `m_instance_extensions` — explicit caller extras.
2. Each feature dependency's `instance_extensions` (flattened, incl. `optional`).
3. **vkc-added platform surface extensions** when `m_enable_presentation` (or any surface dependency is present) — `VK_KHR_surface` + the platform one, chosen by `#ifdef` (§4). This replaces the old `gui::WindowSystem::getRequiredVulkanExtensions()` (SDL) source at `VulkanContext.cpp:94`.

Each is intersected with `vk::enumerateInstanceExtensionProperties()` before use (wish-list semantics, same as device extensions). Layers likewise intersected with `enumerateInstanceLayerProperties()`.

---

## 2. `createContext` flow

```
flatten feature dependencies (expand optional, dedup)
  → assemble instance extensions (§1) ∩ available, layers ∩ available
  → create vk::Instance + debug messenger + instance dispatch        → Context
  → enumerate physical devices
  → for each: evaluate feature support (query FeatureChain once) + device extensions
  → reject only devices missing a required dependency
  → score survivors (supported-dependency count, then scorer)        → chosen device
  → create vk::Device: pNext = enabled FeatureChain.root(),
        extensions = supported device-extension union,
        queues = every queue of every family                          → DeviceContext
  → freeze capability snapshot into DeviceContext
  → build one QueueContext per created queue; build QueueRole table   → DeviceContext.m_queue_table
  → emplace DeviceContext into Context (sorted by score), build DeviceRole table
```

Key point: **no surface, no window, no present query appears anywhere in this flow.** Context creation is fully window-independent on all platforms. This removes the old constraint that windows must be `registerWindow`-ed before `create()` (`VulkanContext.cpp:30-53`).

---

## 3. Decoupling queue selection from surfaces

The old `findQueueFamilies` (`VulkanContext.cpp:141-172`) filters graphics families by `getSurfaceSupportKHR(i, surface)` (line 152), coupling queue/device selection to a live surface → to a window. We remove this entirely.

**Decision: defer present-family selection to swapchain creation.** Because we already create *every* queue of *every* family (full topology), no family needs to be pre-selected for presentation. When a swapchain is created against a real surface, it calls `getSurfaceSupportKHR(family, surface)` across the existing families and picks a present-capable one (usually the eMainGraphics family). This is:
- **surface/window/display-free at context creation** on all platforms;
- strictly more flexible (different surfaces may resolve to different families);
- the place where the `vkQueuePresentKHR` VUID ("queue family must support present to *this* surface") is actually satisfiable, since the surface exists there.

**Why not the platform presentation-support query at context time.** `vkGetPhysicalDevice<Platform>PresentationSupportKHR(pd, family)` can record a per-family present bit without a surface, but the signatures are uneven: `getWin32PresentationSupportKHR(family)` needs only the index, while `getXcbPresentationSupportKHR(family, connection, visual)` / `getWaylandPresentationSupportKHR(family, display)` need the display connection — which would re-introduce a window-system dependency at context creation on Linux. Deferring to swapchain creation sidesteps this on every platform. Keep the platform query only as an optional future upfront-hint, not in the core path.

**Placeholder/headless surface is not needed for decoupling.** `VK_EXT_headless_surface` is a separate, optional tool for CI swapchain tests with no window; it is not part of context creation.

---

## 4. vkc-owned surface creation (replaces SDL)

vkc creates and owns `vk::SurfaceKHR` from a native window handle, per platform — referencing how SDL3's `SDL_Vulkan_CreateSurface` dispatches. The GUI side shrinks to handing vkc an opaque native handle (it can pull these from SDL via `SDL_GetWindowProperties`, or from any custom windowing). This **supersedes** the `SurfaceProvider` callback design in `roadmap/vk_core/deep-dive/swapchain-gui-decoupling.md` and the `gui::VulkanSurfaceBridge` path (`GUI/src/SurfaceBridge.cpp`). → file a vk_core ADR before editing that deep-dive.

### Boundary type: platform-tagged `WindowHandle`

A small struct populated by `#ifdef`, mirroring the data each platform's surface-create-info needs (the same data SDL3 reads from its window):

```cpp
namespace lcf::vkc {
struct WindowHandle {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    void * hinstance;   // HINSTANCE
    void * hwnd;        // HWND
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    void * display;     // wl_display *
    void * surface;     // wl_surface *
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    void * connection;  // xcb_connection_t *
    uint32_t window;    // xcb_window_t
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    const void * layer; // CAMetalLayer *
#endif
};
}
```

### Surface creation, per platform (the SDL3 dispatch, owned by vkc)

| Platform | Instance extension | Create call | Handle source |
|---|---|---|---|
| Win32 | `VK_KHR_win32_surface` | `instance.createWin32SurfaceKHRUnique({{}, hinstance, hwnd})` | `GetModuleHandle` + window HWND |
| Wayland | `VK_KHR_wayland_surface` | `createWaylandSurfaceKHRUnique({{}, display, surface})` | `wl_display` + `wl_surface` |
| Xcb | `VK_KHR_xcb_surface` | `createXcbSurfaceKHRUnique({{}, connection, window})` | xcb connection + window |
| Metal | `VK_EXT_metal_surface` | `createMetalSurfaceEXTUnique({{}, layer})` | `CAMetalLayer` (MoltenVK) |

Plus `VK_KHR_surface` always, when presentation is enabled. These names move into `surf::k_module_dependency.instance_extensions`, selected by the same `#ifdef` — so the surface module's manifest, not the GUI, declares them. A free function:

```cpp
vk::UniqueSurfaceKHR createSurface(vk::Instance, const WindowHandle &);  // surf/ , #ifdef body
```

returns a `vk::UniqueSurfaceKHR` that the `Swapchain` then owns for its lifetime (matching the existing `Swapchain::create(..., vk::UniqueSurfaceKHR)` signature).

### The one real cost: Linux X11 vs Wayland

SDL picks X11/Wayland at runtime; a vkc that compiles one path loses that. The `WindowHandle` variant carries which flavor it is, and vkc keeps both `#ifdef` paths (cheap). The GUI decides which handle to populate. Windows/macOS each have a single platform API, so no such branch.

---

## 5. Swapchain creation (where surface re-enters)

```
caller obtains WindowHandle from GUI
  → surf::createSurface(instance, handle)                  → vk::UniqueSurfaceKHR
  → among DeviceContext queue families, getSurfaceSupportKHR(family, surface)
        → pick present-capable family (validates the VUID here)
  → Swapchain::create(bridge, frames_in_flight, std::move(surface))
```

The `SwapchainBridge` (`surface/Swapchain.h`) keeps only the lifecycle callbacks it still needs (before/after destroy); the `SurfaceCreateFunc`/`SurfaceDestroyFunc` members are no longer needed once vkc owns creation — surface comes in already-created and owned.

---

## 6. Where things live

| Concern | Location | Namespace |
|---|---|---|
| `ContextCreateInfo`, `createContext` | `vk_core/context/` or `vk_core/init/` | `lcf::vkc` / `lcf::vkc::init` |
| `WindowHandle`, `createSurface` | `vk_core/surface/` | `lcf::vkc::surf` |
| surface instance/device extensions | `vk_core/surface/feature_dependencies.h` | `lcf::vkc::surf` |
| per-family present resolution | `Swapchain::create` | `lcf::vkc` |

---

## 7. Net changes vs current code

- `BootstrapConfig` → `ContextCreateInfo` (vk-hpp + project CreateInfo style); `bootstrap` → `createContext`.
- `findQueueFamilies`' `getSurfaceSupportKHR` dependency removed; present-family resolution deferred to `Swapchain::create`.
- `gui::WindowSystem::getRequiredVulkanExtensions()` (SDL) source removed; platform surface instance extensions declared by `surf::k_module_dependency` via `#ifdef`.
- `gui::VulkanSurfaceBridge::createBackend` / `SDL_Vulkan_CreateSurface` removed; `surf::createSurface(instance, WindowHandle)` owns it per platform.
- Supersedes `swapchain-gui-decoupling.md`'s `SurfaceProvider` → **ADR required** in `roadmap/vk_core/decisions/` before that deep-dive is rewritten.
