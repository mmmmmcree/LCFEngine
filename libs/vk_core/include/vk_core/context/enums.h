#pragma once

#include <cstdint>

namespace lcf::vkc::enums {

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
