#pragma once

#include <SDL3/SDL.h>
#include "PointerDefs.h"
#include "GUIEnums.h"
#include <SDL3/SDL_vulkan.h>

namespace lcf::gui {
    class SDLWindowSystem;

    class SDLWindow : public STDPointerDefs<SDLWindow>
    {
        friend class SDLWindowSystem;
        SDLWindow() = default;
    public:
        ~SDLWindow();
        bool create();
        VkSurfaceKHR create(VkInstance instance);
        bool isCreated() const noexcept { return m_window_p and m_state != WindowState::eNotCreated; }
        WindowState getState() const noexcept { return m_state; }
        void pollEvents();
        void show();
        void setFullScreen();
        void hide();
    private:
        SDL_Window * m_window_p = nullptr;
        WindowState m_state = WindowState::eNotCreated;
    };
}