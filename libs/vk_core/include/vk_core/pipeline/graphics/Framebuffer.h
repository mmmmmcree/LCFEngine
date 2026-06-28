#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class FramebufferCreateInfo;

class Framebuffer
{
    using Self = Framebuffer;
public:
    ~Framebuffer() noexcept = default;
    Framebuffer() noexcept = default;
    Framebuffer(const Self &) noexcept = delete;
    Framebuffer(Self &&) noexcept = default;
    Self &operator=(const Self &) noexcept = delete;
    Self &operator=(Self &&) noexcept = default;
private:
};

} // namespace lcf::vkc