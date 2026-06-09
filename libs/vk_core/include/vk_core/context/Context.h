#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class Context
{
    using Self = Context;
public:
    ~Context();
    Context(const Self &) = delete;
    Context(Self &&) = delete;
    Self & operator =(const Self &) = delete;
    Self &  operator =(Self &&) = delete;

private:
    vk::UniqueInstance m_instance;
    //physical devices
    //unique devices
};

} // namespace lcf::vkc