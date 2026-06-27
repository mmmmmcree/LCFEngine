#pragma once

#include <cstdint>
#include <variant>

namespace lcf::win {

struct CloseEvent {};

struct ResizeEvent
{
    uint32_t m_width = 0u;
    uint32_t m_height = 0u;
};

using WindowEvent = std::variant<
    CloseEvent,
    ResizeEvent>;

} // namespace lcf::win
