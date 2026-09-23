#pragma once

#include <cstdint>

namespace lcf::shader_toy {

enum class EventState : std::uint8_t
{
    eNone,
    eRequest,
    eCompletion,
};

} 
