#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class RenderingInfo;

class CommandBufferProxy;

class RenderTarget;

class DynamicRendering
{
public:
    ~DynamicRendering() noexcept = default;
public:
    std::error_code create(vk::Device device, const RenderingInfo & info) noexcept;
    void begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept;
    void end(CommandBufferProxy & cmd) noexcept;
private:
};

} // namespace lcf::vkc