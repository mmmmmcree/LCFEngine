#pragma once

#include <vulkan/vulkan.hpp>
#include <set>
#include <span>
#include <string_view>

namespace lcf::render::vkreq {

    const std::set<std::string_view> & get_instance_extensions() noexcept;

    const std::set<std::string_view> & get_instance_layers() noexcept;

    const std::set<std::string_view> & get_device_extensions() noexcept;

    const std::set<std::string_view> & get_presentation_device_extensions() noexcept;

} // namespace lcf::render::vkreq
