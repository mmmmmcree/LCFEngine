#pragma once

#include <cstdint>
#include "enums/enum_flags.h"

namespace lcf::vkc {

enum class ContextCapabilitiesFlags : uint32_t
{
    eNone = 0,
    eSurface = 1 << 0,
    eAll = eSurface
};

// enum class DeviceRole : uint8_t
// {
//     eMain = 0,
//     eCompute,
// };

// enum class QueueRole : uint8_t
// {
//     eGraphics = 0,
//     eCompute,
//     eTransfer,
// };

} // namespace lcf::vkc

namespace lcf {

template <> inline constexpr bool is_enum_flags_v<vkc::ContextCapabilitiesFlags> = true;

} // namespace lcf
