#pragma once

#include <vulkan/vulkan.hpp>
#include "enums.h"

namespace lcf::vkc {

class CommandBufferAllocateInfo
{
    using Self = CommandBufferAllocateInfo;
public:
    Self & addUsageFlags(CommandBufferUsageFlags flags) noexcept { m_usage_flags |= flags; return *this; }
    Self & setLevel(vk::CommandBufferLevel level) noexcept { m_level = level; return *this; }
    CommandBufferUsageFlags getUsageFlags() const noexcept  { return m_usage_flags; }
    vk::CommandBufferLevel getLevel() const noexcept { return m_level; }
private:
    CommandBufferUsageFlags m_usage_flags = {};
    vk::CommandBufferLevel m_level = vk::CommandBufferLevel::ePrimary;
};

} // namespace lcf::vkc
