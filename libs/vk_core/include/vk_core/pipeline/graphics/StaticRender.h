#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_map>

namespace lcf::vkc {

class StaticRenderInfo;
class RenderTarget;
class CommandBufferProxy;

class StaticRenderScopeInfo
{
public:
    ~StaticRenderScopeInfo() noexcept = default;
    StaticRenderScopeInfo(vk::RenderPass render_pass, uint32_t subpass_index) noexcept :
        m_render_pass(render_pass), m_subpass_index(subpass_index) {}
    StaticRenderScopeInfo(const StaticRenderScopeInfo &) = default;
    StaticRenderScopeInfo(StaticRenderScopeInfo &&) noexcept = default;
    StaticRenderScopeInfo & operator=(const StaticRenderScopeInfo &) = default;
    StaticRenderScopeInfo & operator=(StaticRenderScopeInfo &&) noexcept = default;
public:
    const vk::RenderPass & getRenderPass() const noexcept { return m_render_pass; }
    const uint32_t & getSubpassIndex() const noexcept { return m_subpass_index; }
private:
    vk::RenderPass m_render_pass;
    uint32_t m_subpass_index;
};

class StaticRender
{
    using FramebufferCache = std::unordered_map<std::uint64_t, vk::UniqueFramebuffer>;
public:
    std::error_code create(vk::Device device, const StaticRenderInfo & render_info) noexcept;
    void begin(CommandBufferProxy & cmd, const RenderTarget & render_target) noexcept;
    void end(CommandBufferProxy & cmd) noexcept;
    const vk::RenderPass & getRenderPass() const noexcept { return m_render_pass.get(); }
    StaticRenderScopeInfo makeScopeInfo(uint32_t subpass_index) const noexcept { return {this->getRenderPass(), subpass_index}; }
private:
    vk::Device m_device;
    vk::UniqueRenderPass m_render_pass;
    FramebufferCache m_framebuffer_cache;
};

} // namespace lcf::vkc