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

    enum class SurfaceState
    {
        eNotCreated, //- initial state
        eActive, //- set by front-end, initialised by back-end
        eSilent, //- set by front-end
        eAboutToDestroy, //- set by front-end
        eDestroyed, //- set by back-end
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