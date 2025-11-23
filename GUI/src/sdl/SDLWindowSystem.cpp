#include "sdl/SDLWindowSystem.h"
#include <SDL3/SDL_vulkan.h>
#include "gui_types.h"
#include "gui_serialization.h"

using namespace lcf::gui;

DisplayModeInfo generate_display_mode_info(const SDL_DisplayMode & mode);

DisplayerInfo generate_displayer_info(SDL_DisplayID id);

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

SDLWindowSystem::DisplayerInfoList lcf::gui::SDLWindowSystem::getDisplayerInfoList() const
{
    int displayer_count;
    SDL_DisplayID *display_id_list = SDL_GetDisplays(&displayer_count);
    DisplayerInfoList displayer_info_list; displayer_info_list.reserve(displayer_count);
    for (int i = 0; i < displayer_count; ++i) {
        displayer_info_list.emplace_back(generate_displayer_info(display_id_list[i]));
    }
    return displayer_info_list;
}

DisplayerInfo lcf::gui::SDLWindowSystem::getPrimaryDisplayerInfo() const
{
    return generate_displayer_info(SDL_GetPrimaryDisplay());
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

DisplayModeInfo generate_display_mode_info(const SDL_DisplayMode &mode)
{
    return DisplayModeInfo {
        static_cast<uint32_t>(mode.w),
        static_cast<uint32_t>(mode.h),
        mode.pixel_density,
        mode.refresh_rate
    };
}

DisplayerInfo generate_displayer_info(SDL_DisplayID id)
{
    const char * name = SDL_GetDisplayName(id);
    const SDL_DisplayMode * current_mode = SDL_GetCurrentDisplayMode(id);
    const SDL_DisplayMode * desktop_mode = SDL_GetDesktopDisplayMode(id);
    int mode_count;
    SDL_DisplayMode ** modes = SDL_GetFullscreenDisplayModes(id, &mode_count);
    DisplayModeInfo current_mode_info = generate_display_mode_info(*current_mode);
    DisplayModeInfo desktop_mode_info = generate_display_mode_info(*desktop_mode);
    std::vector<DisplayModeInfo> available_modes; available_modes.reserve(mode_count);
    for (int i = 0; i < mode_count; ++i) {
        available_modes.emplace_back(generate_display_mode_info(*modes[i]));
    }
    return DisplayerInfo {
        id == SDL_GetPrimaryDisplay(),
        id,
        name,
        desktop_mode_info,
        current_mode_info,
        std::move(available_modes)
    };
}
