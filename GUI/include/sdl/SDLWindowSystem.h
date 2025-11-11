#pragma once

#include "sdl/SDLWindow.h"
#include <vector>
#include <string>
#include <set>

namespace lcf::gui {
    class SDLWindowSystem
    {
        friend class SDLWindow;
        SDLWindowSystem();
        using WindowPtrSet = std::set<SDLWindow *>;
    public:
        static SDLWindowSystem & getInstance();
        ~SDLWindowSystem();
        SDLWindow::UniquePointer allocateWindow();
        std::vector<std::string> getRequiredVulkanExtensions() const;
    private:
        void deallocateWindow(SDLWindow * window_p);
    private:
        WindowPtrSet m_window_ptr_set;
    };
}