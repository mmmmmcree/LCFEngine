#pragma once

#include "PointerDefs.h"
#include "Entity.h"
#include "gui_forward_declares.h"
#include "gui_enums.h"
#include <SDL3/SDL.h>
#include <atomic>

namespace lcf::gui {
    class SDLWindowSystem;

    class SDLWindow : public STDPointerDefs<SDLWindow>
    {
        friend class SDLWindowSystem;
        SDLWindow() = default;
    public:
        ~SDLWindow();
        bool create();
        bool create(const WindowCreateInfo & info);
        Entity & getEntity() noexcept { return m_entity; }
        const Entity & getEntity() const noexcept { return m_entity; }
        bool isCreated() const noexcept { return m_window_p and m_state != WindowState::eNotCreated; }
        WindowState getState() const noexcept { return m_state; }
        void pollEvents();
        void show();
        void setFullScreen();
        void hide();
    private:
        void setSurfaceState(SurfaceState state) noexcept;
    private:
        SDL_Window * m_window_p = nullptr;
        Entity m_entity;
        WindowState m_state = WindowState::eNotCreated;
    };

}