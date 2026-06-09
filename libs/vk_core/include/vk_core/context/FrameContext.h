#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class FrameContext
{
    using Self = FrameContext;
public:
    ~FrameContext() = default;
    FrameContext(const Self &) = delete;
    FrameContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;

private:
    //all resources that one frame needs
};

} // namespace lcf::vkc