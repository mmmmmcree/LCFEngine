#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <span>
#include <system_error>

namespace lcf::vkc {

class RenderingInfo;
class RenderTargetInfo;
class RenderTarget;
class CommandBufferProxy;

class StaticRendering
{
    using FramebufferCache = std::unordered_map<std::uint64_t, vk::UniqueFramebuffer>;
public:
    ~StaticRendering() noexcept = default;
public:
    std::error_code create(
        vk::Device device,
        const RenderingInfo & rendering_info,
        const RenderTargetInfo & render_target_info) noexcept;
    void begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept;
    void end(CommandBufferProxy & cmd) noexcept;
private:
    vk::UniqueRenderPass m_render_pass;
    FramebufferCache m_framebuffer_cache;
};

} // namespace lcf::vkc