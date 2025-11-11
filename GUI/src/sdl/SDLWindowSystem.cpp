#include "sdl/SDLWindowSystem.h"
#include <SDL3/SDL_vulkan.h>

using namespace lcf::gui;

SDLWindowSystem & SDLWindowSystem::getInstance()
{
    static SDLWindowSystem s_instance;
    return s_instance;
}

lcf::gui::SDLWindowSystem::SDLWindowSystem()
{
    SDL_Init(SDL_INIT_VIDEO);
}

lcf::gui::SDLWindowSystem::~SDLWindowSystem()
{
    SDL_Quit();
}

SDLWindow::UniquePointer lcf::gui::SDLWindowSystem::allocateWindow()
{
    auto window_p = new SDLWindow;
    m_window_ptr_set.insert(window_p);
    return SDLWindow::UniquePointer(window_p);
}

std::vector<std::string> lcf::gui::SDLWindowSystem::getRequiredVulkanExtensions() const
{
    std::vector<std::string> extensions;
    if (m_window_ptr_set.empty()) { return extensions; } // headless surface ?
    uint32_t count;
    auto sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    for (uint32_t i = 0; i < count; ++i) {
        extensions.emplace_back(sdl_extensions[i]);
    }
    return extensions;
}

void lcf::gui::SDLWindowSystem::deallocateWindow(SDLWindow * window_p)
{
    m_window_ptr_set.erase(window_p);
}
