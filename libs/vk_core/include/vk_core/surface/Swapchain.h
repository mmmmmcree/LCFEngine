#pragma once

#include <vulkan/vulkan.hpp>
#include <functional>
#include "enums.h"

namespace lcf::vkc {

struct SwapchainBridge
{
    using Callback = std::function<void(void)>;
    using SurfaceCreateFunc = std::function<vk::SurfaceKHR(vk::Instance)>;
    using SurfaceDestroyFunc = std::function<void(vk::SurfaceKHR)>;
    // using SurfaceStatusGetter = std::function<enums::SurfaceStatus(void)>;
    // using StatusChangedCallback = std::function<void(enums::SurfaceStatus)>;

    void onBeforeDestroyed() noexcept { m_before_destroyed(); }
    void onAfterDestroyed() noexcept { m_after_destroyed(); }
    // void onSurfaceStatusChanged(enums::SurfaceStatus status) noexcept { m_surface_status_changed(status); }
    // enums::SurfaceStatus getSurfaceStatus() noexcept { return m_get_surface_status(); }

    Callback m_before_destroyed;
    Callback m_after_destroyed;
    // StatusChangedCallback m_surface_status_changed;
    // SurfaceStatusGetter m_get_surface_status;
};

class Swapchain
{
public:
    Swapchain();
    std::error_code create(SwapchainBridge bridge, uint32_t frame_in_flight, vk::UniqueSurfaceKHR surface);
private:
};

} // namespace lcf::vkc