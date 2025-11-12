#pragma once

#include <cstdint>

namespace lcf::gui {
    enum class SurfaceType : uint8_t
    {
        eNone,
        eRaster,
        eOpenGL,
        eVulkan,
        eMetal,
        eDirectX,
    };

    enum class WindowState
    {
        eNotCreated,
        eHidden,
        eNormal,
        eFullScreen,
        eAboutToClose,
    };
}