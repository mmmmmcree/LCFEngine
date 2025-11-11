#pragma once

namespace lcf::gui {
    enum class WindowState
    {
        eNotCreated,
        eHidden,
        eNormal,
        eFullScreen,
        eAboutToClose,
    };
}