#pragma once

#include <cstdint>
#include "enums/enum_flags.h"

namespace lcf::vkc::enums {

enum class ContextCapabilitiesFlags : uint32_t
{
    eNone = 0,
    eSurface = 1 << 0,
    eAll = eSurface
};

enum class DeviceRole : uint8_t
{
    eMain = 0,
    eCompute,
};

enum class QueueRole : uint8_t
{
    eMainGraphics = 0,
    eSubGraphics,
    eCompute,
    eTransfer,
};

} // namespace lcf::vkc::enums

namespace lcf {

template <> inline constexpr bool is_enum_flags_v<vkc::enums::ContextCapabilitiesFlags> = true;

} // namespace lcf
