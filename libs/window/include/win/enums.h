#pragma once

#include <cstdint>
#include "enums/enum_flags.h"

namespace lcf::win {

enum class WindowFlags : uint32_t
{
    eNone       = 0x0,
    eResizable  = 0x1,
    eBorderless = 0x2,
    eHidden     = 0x4,
    eFullscreen = 0x8,
};

} // namespace lcf::win

namespace lcf {
    template <> inline constexpr bool is_enum_flags_v<win::WindowFlags> = true;
}
