#include "SurfaceBridge.h"

// #if defined(SDL_WINDOW)
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_log.h>

void lcf::gui::VulkanSurfaceBridge::createBackend(VkInstance instance)
{
    SDL_Vulkan_CreateSurface(m_window_p, instance, nullptr, &m_surface);
    if (not m_surface) {
        SDL_Log("lcf::gui::VulkanSurfaceBackendBridge creation failed: %s", SDL_GetError());
    }
}