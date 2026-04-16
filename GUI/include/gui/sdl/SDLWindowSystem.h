#pragma once

#include "gui/gui_fwd_decls.h"
#include "gui/sdl/SDLWindow.h"
#include <vector>
#include <span>
#include <set>

namespace lcf::gui {
    class SDLWindowSystem
    {
        friend class SDLWindow;
        using DisplayerInfoList = std::vector<DisplayerInfo>;
        using WindowPtrSet = std::set<SDLWindow *>;
    public:
        static SDLWindowSystem & getInstance();
        ~SDLWindowSystem();
        DisplayerInfoList getDisplayerInfoList() const;
        DisplayerInfo getPrimaryDisplayerInfo() const;
        SDLWindow::UniquePointer allocateWindow();
        std::span<const char * const> getRequiredVulkanExtensions() const;
    private:
        SDLWindowSystem();
        void deallocateWindow(SDLWindow * window_p);
    private:
        WindowPtrSet m_window_ptr_set;
    };
}