#pragma once

#include <cstddef>
#include <function2/function2.hpp>

namespace lcf::shader_toy {

inline constexpr std::size_t k_unique_function_capacity = 64;

template <typename Signature>
using UniqueFunction = fu2::function_base<
    true,
    false,
    fu2::capacity_fixed<k_unique_function_capacity>,
    false,
    false,
    Signature
>;

} // namespace lcf::shader_toy
