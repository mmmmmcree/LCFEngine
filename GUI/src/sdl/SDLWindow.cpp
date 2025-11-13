#include "sdl/SDLWindow.h"
#include "sdl/SDLWindowSystem.h"
#include <SDL3/SDL_vulkan.h>
#include "SurfaceBridge.h" 
#include "gui_types.h"

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

bool lcf::gui::SDLWindow::create(const WindowCreateInfo &info)
{
    if (this->isCreated()) { return false; }
    auto registry_p = info.getRegistryPtr();
    if (not registry_p) { return false; }
    m_entity.setRegistry(*registry_p);
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN; //todo temp
    switch (info.getSurfaceType()) {
        case SurfaceType::eVulkan: { flags |= SDL_WINDOW_VULKAN; } break;
    }
    m_window_p = SDL_CreateWindow(info.getTitle().c_str(), info.getWidth(), info.getHeight(), flags);
    if (not m_window_p) {
        SDL_Log("lcf::gui::SDLWindow creation failed: %s", SDL_GetError());
        return false;
    }
    switch (info.getSurfaceType()) {
        case SurfaceType::eVulkan: {
            auto & bridge_sp = m_entity.requireComponent<VulkanSurfaceBridge::SharedPointer>();
            bridge_sp = VulkanSurfaceBridge::makeShared();
            bridge_sp->createFrontend(m_window_p);
        } break;
    }
    m_state = WindowState::eHidden;
    return this->isCreated();
}

void lcf::gui::SDLWindow::pollEvents()
{
    for (SDL_Event event; SDL_PollEvent(&event);) {
        switch (event.type) {
            case SDL_EVENT_QUIT: {
                m_state = WindowState::eAboutToClose;
                this->setSurfaceState(SurfaceState::eAboutToDestroy);
            } break;
        }
    }
}

void lcf::gui::SDLWindow::show() 
{
    if (m_state != WindowState::eHidden) { return; }
    SDL_ShowWindow(m_window_p);
    m_state = WindowState::eNormal;
    this->setSurfaceState(SurfaceState::eActive);
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
    this->setSurfaceState(SurfaceState::eSilent);
}

void lcf::gui::SDLWindow::setSurfaceState(SurfaceState state) noexcept
{
    auto & bridge_sp = m_entity.getComponent<VulkanSurfaceBridge::SharedPointer>();
    if (bridge_sp->getState() == SurfaceState::eNotCreated) { return; }
    bridge_sp->setState(state);
}
