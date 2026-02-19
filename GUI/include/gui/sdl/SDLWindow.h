#pragma once

#include "PointerDefs.h"
#include "Entity.h"
#include "gui/gui_fwd_decls.h"
#include "gui/gui_enums.h"
#include <SDL3/SDL.h>
#include "InputState.h"

namespace lcf::gui {
    class SDLWindowSystem;

    class SDLWindow : public STDPointerDefs<SDLWindow>
    {
        friend class SDLWindowSystem;
        SDLWindow() = default;
    public:
        ~SDLWindow();
        bool create(const WindowCreateInfo & info);
        Entity & getEntity() noexcept { return m_entity; }
        const Entity & getEntity() const noexcept { return m_entity; }
        template <typename Component>
        Component & getComponent() const { return m_entity.getComponent<Component>(); }
        bool isCreated() const noexcept { return m_window_p and m_state != WindowState::eNotCreated; }
        WindowState getState() const noexcept { return m_state; }
        void pollEvents() noexcept;
        void show();
        void setFullScreen();
        void hide();
    private:
        void setSurfaceState(SurfaceState state) noexcept;
    private:
        SDL_Window * m_window_p = nullptr;
        Entity m_entity;
        WindowState m_state = WindowState::eNotCreated;
        InputState m_input_state;
    };

}