#include "vk_core/pipeline/graphics/StaticRender.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

std::error_code StaticRender::create(vk::Device device, const StaticRenderInfo & render_info) noexcept
{
    m_device = device;
    auto render_pass_info = static_cast<const vk::RenderPassCreateInfo2 &>(render_info);
    auto subpasses = render_info.getSubpasses() |
        stdv::transform([](const auto &subpass) { return static_cast<vk::SubpassDescription2>(subpass); }) |
        stdr::to<std::vector>();
    auto dependencies = render_info.getDependencies() |
        stdv::transform([](const auto &dependency) { return static_cast<vk::SubpassDependency2>(dependency); }) |
        stdr::to<std::vector>();
    auto attachments = render_info.getAttachmentDescriptions() |
        stdv::transform([](const auto &desc) { return static_cast<vk::AttachmentDescription2>(desc); }) |
        stdr::to<std::vector>();
    
    render_pass_info.setAttachments(attachments)
        .setSubpasses(subpasses)
        .setDependencies(dependencies)
        .setCorrelatedViewMasks(render_info.getCorrelatedViewMasks());
    try {
        m_render_pass = device.createRenderPass2Unique(render_pass_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

void StaticRender::begin(CommandBufferProxy & cmd, const RenderTarget & render_target) noexcept
{
    vk::RenderPassBeginInfo render_pass_info;
    std::uint64_t target_hash = reinterpret_cast<std::uint64_t>(&render_target);
    auto it = m_framebuffer_cache.find(target_hash);
    if (it == m_framebuffer_cache.end()) {
        auto [width, height] = render_target.getMaxExtent();
        auto attachments = render_target.viewAttachmentImageViews() | stdr::to<std::vector>();
        vk::FramebufferCreateInfo framebuffer_info;
        framebuffer_info.setRenderPass(m_render_pass.get())
            .setWidth(width)
            .setHeight(height)
            .setLayers(render_target.getLayerCount())
            .setAttachments(attachments);
        it = m_framebuffer_cache.emplace(target_hash, m_device.createFramebufferUnique(framebuffer_info)).first;
    }
    vk::Framebuffer framebuffer = it->second.get();
        
    render_pass_info.setRenderPass(m_render_pass.get())
        .setFramebuffer(framebuffer)
        .setRenderArea(render_target.getRenderArea())
        .setClearValues(render_target.getClearValues());
    cmd.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
}

void StaticRender::end(CommandBufferProxy &cmd) noexcept
{
    cmd.endRenderPass();
}

} // namespace lcf::vkc