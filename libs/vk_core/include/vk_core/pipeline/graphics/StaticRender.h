#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_map>

namespace lcf::vkc {

class StaticRenderInfo;
class RenderTarget;
class CommandBufferProxy;

class StaticRender
{
    using FramebufferCache = std::unordered_map<std::uint64_t, vk::UniqueFramebuffer>;
public:
    std::error_code create(vk::Device device, const StaticRenderInfo & info) noexcept;
    void begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept;
    void end(CommandBufferProxy & cmd) noexcept;
    const vk::RenderPass & getRenderPass() const noexcept { return m_render_pass.get(); }
private:
    vk::Device m_device;
    vk::UniqueRenderPass m_render_pass;
    FramebufferCache m_framebuffer_cache;
};

} // namespace lcf::vkc