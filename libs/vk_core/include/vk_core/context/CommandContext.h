#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class CommandContext
{
    using Self = CommandContext;
public:
    ~CommandContext() = default;
    CommandContext(const Self &) = delete;
    CommandContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;

private:
    vk::CommandBuffer m_cmd;
};

} // namespace lcf::vkc