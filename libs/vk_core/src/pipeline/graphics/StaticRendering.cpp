#include "vk_core/pipeline/graphics/StaticRendering.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

std::error_code StaticRendering::create(
    vk::Device device,
    const RenderingInfo & rendering_info,
    const RenderTargetInfo & render_target_info) noexcept
{
    m_device = device;
    vk::RenderPassCreateInfo2 render_pass_info;
    auto subpasses = rendering_info.getSubpasses() |
        stdv::transform([](const auto &subpass) { return static_cast<vk::SubpassDescription2>(subpass); }) |
        stdr::to<std::vector>();
    auto dependencies = rendering_info.getDependencies() |
        stdv::transform([](const auto &dependency) { return static_cast<vk::SubpassDependency2>(dependency); }) |
        stdr::to<std::vector>();
    auto attachment_descs = stdv::zip(rendering_info.getAttachmentStates(), render_target_info.getColorFormats()) |
        stdv::transform([sample_count = render_target_info.getSampleCount()](auto && input) -> vk::AttachmentDescription2 {
            auto && [state, format] = std::forward<decltype(input)>(input);
            return vk::AttachmentDescription2 {}.setFormat(format)
                .setSamples(sample_count)
                .setLoadOp(state.getLoadOp())
                .setStoreOp(state.getStoreOp())
                .setStencilLoadOp(state.getStencilLoadOp())
                .setStencilStoreOp(state.getStencilStoreOp())
                .setInitialLayout(state.getInitialLayout())
                .setFinalLayout(state.getFinalLayout());
        }) | stdr::to<std::vector>();
    render_pass_info.setAttachments(attachment_descs)
        .setSubpasses(subpasses)
        .setDependencies(dependencies)
        .setCorrelatedViewMasks(rendering_info.getCorrelatedViewMasks());
    try {
        m_render_pass = device.createRenderPass2Unique(render_pass_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

void StaticRendering::begin(CommandBufferProxy &cmd, const RenderTarget &target) noexcept
{
    vk::RenderPassBeginInfo render_pass_info;
    std::uint64_t target_hash = reinterpret_cast<std::uint64_t>(&target);
    auto it = m_framebuffer_cache.find(target_hash);
    if (it == m_framebuffer_cache.end()) {
        auto [width, height] = target.getMaxExtent();
        auto attachments = target.viewAttachmentImageViews() | stdr::to<std::vector>();
        vk::FramebufferCreateInfo framebuffer_info;
        framebuffer_info.setRenderPass(m_render_pass.get())
            .setWidth(width)
            .setHeight(height)
            .setLayers(target.getLayerCount())
            .setAttachments(attachments);
        it = m_framebuffer_cache.emplace(target_hash, m_device.createFramebufferUnique(framebuffer_info)).first;
    }
    vk::Framebuffer framebuffer = it->second.get();
        
    render_pass_info.setRenderPass(m_render_pass.get())
        .setFramebuffer(framebuffer)
        .setRenderArea(target.getRenderArea())
        .setClearValues(target.getClearValues());
    cmd.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
}

void StaticRendering::end(CommandBufferProxy &cmd) noexcept
{
    cmd.endRenderPass();
}

} // namespace lcf::vkc