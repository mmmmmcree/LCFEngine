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
    Self & setCount(uint32_t count) noexcept { m_count = std::max(count, 1u); return *this; }
    CommandBufferUsageFlags getUsageFlags() const noexcept  { return m_usage_flags; }
    vk::CommandBufferLevel getLevel() const noexcept { return m_level; }
    uint32_t getCount() const noexcept { return m_count; }
private:
    CommandBufferUsageFlags m_usage_flags = {};
    vk::CommandBufferLevel m_level = vk::CommandBufferLevel::ePrimary;
    uint32_t m_count = 1u;
};

} // namespace lcf::vkc
