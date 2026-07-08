#pragma once

#include <concepts>

namespace lcf {

template <typename T>
concept member_pointer_c = std::is_member_pointer_v<T>;

} // namespace lcf