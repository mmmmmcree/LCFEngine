#pragma once

#include "gui_fwd_decls.h"
#include "gui_enums.h"
#include "PointerDefs.h"

class VkInstance_T;
class VkSurfaceKHR_T;

namespace lcf::gui {
    class VulkanSurfaceBridge : public STDPointerDefs<VulkanSurfaceBridge>
    {
    public:
        VulkanSurfaceBridge() = default;
        void createFrontend(WINDOW_IMPL_DECLARE * window_p) noexcept { m_window_p = window_p; }
        void createBackend(VkInstance_T * instance);
        void setState(SurfaceState state) noexcept { m_state.store(state, std::memory_order_release); }
        SurfaceState getState() const noexcept { return m_state.load(std::memory_order_acquire); }
        VkSurfaceKHR_T * getSurface() const noexcept { return m_surface; }
    private:
        WINDOW_IMPL_DECLARE * m_window_p = nullptr;
        VkSurfaceKHR_T * m_surface = nullptr;
        std::atomic<SurfaceState> m_state = SurfaceState::eNotCreated;
    };
}