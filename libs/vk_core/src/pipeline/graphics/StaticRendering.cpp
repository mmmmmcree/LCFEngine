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
    vk::RenderPassCreateInfo2 render_pass_info;
    auto subpasses = rendering_info.getSubpasses() |
        stdv::transform([](const auto &subpass) { return static_cast<vk::SubpassDescription2>(subpass); }) |
        stdr::to<std::vector>();
    auto dependencies = rendering_info.getDependencies() |
        stdv::transform([](const auto &dependency) { return static_cast<vk::SubpassDependency2>(dependency); }) |
        stdr::to<std::vector>();

    // render_pass_info.setAttachments()
    render_pass_info
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
    vk::Framebuffer framebuffer; // get from cache
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