#include "sdl/SDLWindow.h"
#include "sdl/SDLWindowSystem.h"

lcf::gui::SDLWindow::~SDLWindow()
{
    SDL_DestroyWindow(m_window_p);
    m_window_p = nullptr;
    SDLWindowSystem::getInstance().deallocateWindow(this);
}

bool lcf::gui::SDLWindow::create()
{
    if (this->isCreated()) { return false; }
    m_window_p = SDL_CreateWindow("temp name", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    if (m_window_p) {
        m_state = WindowState::eHidden;
    }
    return this->isCreated();
}

VkSurfaceKHR lcf::gui::SDLWindow::create(VkInstance instance)
{
    if (this->isCreated()) { return nullptr; }
    m_window_p = SDL_CreateWindow("temp name", 800, 600,
        SDL_WINDOW_VULKAN |
        SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIDDEN
    );
    if (not m_window_p) {
        const char* error = SDL_GetError();
        SDL_Log("Window creation failed: %s", error);
        return nullptr;
    }
    m_state = WindowState::eHidden;
    VkSurfaceKHR surface;
    return SDL_Vulkan_CreateSurface(m_window_p, instance, nullptr, &surface) ? surface : nullptr;
}

void lcf::gui::SDLWindow::pollEvents()
{
    for (SDL_Event event; SDL_PollEvent(&event);) {
        switch (event.type) {
            case SDL_EVENT_QUIT: { m_state = WindowState::eAboutToClose; } break;
        }
    }
}

void lcf::gui::SDLWindow::show() 
{
    if (m_state != WindowState::eHidden) { return; }
    SDL_ShowWindow(m_window_p);
    m_state = WindowState::eNormal;
}

void lcf::gui::SDLWindow::setFullScreen()
{
    if (m_state == WindowState::eFullScreen) { return; }
    SDL_SetWindowFullscreen(m_window_p, SDL_WINDOW_FULLSCREEN);
    m_state = WindowState::eFullScreen;
}

void lcf::gui::SDLWindow::hide()
{
    if (m_state == WindowState::eHidden) { return; }
    SDL_HideWindow(m_window_p);
    m_state = WindowState::eHidden;
}