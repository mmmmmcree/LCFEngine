#include "sdl/SDLWindow.h"
#include "sdl/SDLWindowSystem.h"
#include <SDL3/SDL_vulkan.h>
#include "SurfaceBridge.h" 
#include "gui_types.h"
#include "InputCollector.h"
#include "input_enums.h"

using namespace lcf;
using namespace lcf::gui;

constexpr KeyboardKey to_enum(SDL_Keycode key) noexcept;

constexpr MouseButtonFlags to_enum(Uint8 button) noexcept;

lcf::gui::SDLWindow::~SDLWindow()
{
    SDL_DestroyWindow(m_window_p);
    m_window_p = nullptr;
    SDLWindowSystem::getInstance().deallocateWindow(this);
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
    // SDL_SetWindowRelativeMouseMode(m_window_p, true); //infinite mouse movement
    switch (info.getSurfaceType()) {
        case SurfaceType::eVulkan: {
            auto & bridge_sp = m_entity.requireComponent<VulkanSurfaceBridge::SharedPointer>();
            bridge_sp = VulkanSurfaceBridge::makeShared();
            bridge_sp->createFrontend(m_window_p);
        } break;
    }
    m_entity.requireComponent<InputCollector>();
    m_state = WindowState::eHidden;
    return this->isCreated();
}

void lcf::gui::SDLWindow::pollEvents()
{
    for (SDL_Event event; SDL_PollEvent(&event);) {
        event.motion;
        switch (event.type) {
            case SDL_EVENT_QUIT: {
                m_state = WindowState::eAboutToClose;
                this->setSurfaceState(SurfaceState::eAboutToDestroy);
            } break;
            case SDL_EVENT_KEY_DOWN: {
                m_input_state.pressKey(to_enum(event.key.key));
            } break;
            case SDL_EVENT_KEY_UP: {
                m_input_state.releaseKey(to_enum(event.key.key));
            } break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                m_input_state.pressMouseButtons(to_enum(event.button.button));                
                if (event.button.clicks == 2) {
                    m_input_state.pressMouseButtons(MouseButtonFlags::eDoubleClicked);
                }
            } break;
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                m_input_state.releaseMouseButtons(to_enum(event.button.button));
                m_input_state.releaseMouseButtons(MouseButtonFlags::eDoubleClicked);
            } break;
            case SDL_EVENT_MOUSE_WHEEL: {
                m_input_state.addWheelOffset({event.wheel.integer_x, event.wheel.integer_y});
            } break;
            case SDL_EVENT_MOUSE_MOTION: {
                m_input_state.addMousePosition({event.motion.xrel, event.motion.yrel});
            } break;
        }
    }

    auto & input_collector = m_entity.getComponent<InputCollector>();
    input_collector.collect(m_input_state);
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

constexpr KeyboardKey to_enum(SDL_Keycode key) noexcept
{
    switch (key) {
        case SDLK_A: { return KeyboardKey::eA; }
        case SDLK_B: { return KeyboardKey::eB; }
        case SDLK_C: { return KeyboardKey::eC; }
        case SDLK_D: { return KeyboardKey::eD; }
        case SDLK_E: { return KeyboardKey::eE; }
        case SDLK_F: { return KeyboardKey::eF; }
        case SDLK_G: { return KeyboardKey::eG; }
        case SDLK_H: { return KeyboardKey::eH; }
        case SDLK_I: { return KeyboardKey::eI; }
        case SDLK_J: { return KeyboardKey::eJ; }
        case SDLK_K: { return KeyboardKey::eK; }
        case SDLK_L: { return KeyboardKey::eL; }
        case SDLK_M: { return KeyboardKey::eM; }
        case SDLK_N: { return KeyboardKey::eN; }
        case SDLK_O: { return KeyboardKey::eO; }
        case SDLK_P: { return KeyboardKey::eP; }
        case SDLK_Q: { return KeyboardKey::eQ; }
        case SDLK_R: { return KeyboardKey::eR; }
        case SDLK_S: { return KeyboardKey::eS; }
        case SDLK_T: { return KeyboardKey::eT; }
        case SDLK_U: { return KeyboardKey::eU; }
        case SDLK_V: { return KeyboardKey::eV; }
        case SDLK_W: { return KeyboardKey::eW; }
        case SDLK_X: { return KeyboardKey::eX; }
        case SDLK_Y: { return KeyboardKey::eY; }
        case SDLK_Z: { return KeyboardKey::eZ; }
        case SDLK_0: { return KeyboardKey::e0; }
        case SDLK_1: { return KeyboardKey::e1; }
        case SDLK_2: { return KeyboardKey::e2; }        
        case SDLK_3: { return KeyboardKey::e3; }
        case SDLK_4: { return KeyboardKey::e4; }
        case SDLK_5: { return KeyboardKey::e5; }
        case SDLK_6: { return KeyboardKey::e6; }
        case SDLK_7: { return KeyboardKey::e7; }
        case SDLK_8: { return KeyboardKey::e8; }
        case SDLK_9: { return KeyboardKey::e9; }
    }
    return KeyboardKey::eUnknown;
}

constexpr MouseButtonFlags to_enum(Uint8 button) noexcept
{
    switch (button) {
        case SDL_BUTTON_LEFT: { return MouseButtonFlags::eLeftButton; }
        case SDL_BUTTON_RIGHT: { return MouseButtonFlags::eRightButton; }
        case SDL_BUTTON_MIDDLE: { return MouseButtonFlags::eMiddleButton; }
        case SDL_BUTTON_X1: { return MouseButtonFlags::eXButton1; }
        case SDL_BUTTON_X2: { return MouseButtonFlags::eXButton2; }
    }
    return MouseButtonFlags::eNoButton;
}
