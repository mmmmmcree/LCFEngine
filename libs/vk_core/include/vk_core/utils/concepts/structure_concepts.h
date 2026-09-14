#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc::utils {

template <typename T, typename Root>
concept struct_extends_c = static_cast<bool>(vk::StructExtends<T, Root>::value);

template <typename T, typename... Roots>
concept struct_extends_any_c = (struct_extends_c<T, Roots> or ...);

} // namespace lcf::vkc::utils
