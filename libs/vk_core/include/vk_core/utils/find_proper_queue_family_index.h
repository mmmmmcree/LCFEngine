#pragma once

#include <vulkan/vulkan.hpp>
#include <span>
#include <optional>

namespace lcf::vkc::utils {

inline std::optional<uint32_t> find_proper_queue_family_index(
    std::span<const vk::QueueFamilyProperties> properties,
    vk::QueueFlags desired_flags,
    vk::QueueFlags undesired_flags) noexcept
{
    std::optional<uint32_t> found_family_index = std::nullopt;
    for (const auto & [family_index, props] : properties | std::views::enumerate) {
        if (not (props.queueFlags & desired_flags)) { continue; }
        found_family_index = static_cast<uint32_t>(family_index);
        if (not (props.queueFlags & undesired_flags)) { break; }
    }
    return found_family_index;
}

} // namespace lcf::vkc::utils