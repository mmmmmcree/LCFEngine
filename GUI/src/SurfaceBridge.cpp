#include "gui/SurfaceBridge.h"

#ifdef USE_SDL3
#include <SDL3/SDL_log.h>
#ifdef USE_VULKAN
#include <SDL3/SDL_vulkan.h>
#endif // USE_VULKAN
#endif // USE_SDL3

void lcf::gui::VulkanSurfaceBridge::createBackend(VkInstance instance)
{
#if defined(USE_SDL3) && defined(USE_VULKAN)
    SDL_Vulkan_CreateSurface(m_window_p, instance, nullptr, &m_surface);
    if (not m_surface) {
        SDL_Log("lcf::gui::VulkanSurfaceBackendBridge creation failed: %s", SDL_GetError());
    }
#endif // USE_SDL3 && USE_VULKAN
}