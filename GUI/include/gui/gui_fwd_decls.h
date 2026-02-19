#pragma once

#include "PointerDefs.h"

// common types
namespace lcf::gui {
    struct WindowCreateInfo;
    struct DisplayModeInfo;
    struct DisplayerInfo;
}

// specific window related
class SDL_Window;
namespace lcf::gui {
    class SDLWindow;
    class SDLWindowSystem;

    #define WINDOW_IMPL_DECL SDL_Window
    #define WINDOW_DECL SDLWindow
    #define WINDOW_SYSTEM_DECL SDLWindowSystem
}

// specific graphics api related
namespace lcf::gui {
    class VulkanSurfaceBridge;
    LCF_DECLARE_POINTER_DEFS(VulkanSurfaceBridge, STDPointerDefs);
}