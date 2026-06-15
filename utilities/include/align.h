#pragma once

#include <concepts>

namespace lcf {

template <std::integral Int>
inline constexpr Int align_up(Int value, std::size_t alignment) noexcept
{
    return static_cast<Int>(((value + (alignment - 1)) & ~(alignment - 1)));
}

} // namespace lcf