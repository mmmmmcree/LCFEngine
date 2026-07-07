#include "vk_core/pipeline/graphics/StaticRendering.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/command/CommandBufferProxy.h"

namespace lcf::vkc {


std::error_code StaticRendering::create(vk::Device device, const RenderingInfo & info) noexcept
{
    return std::error_code();
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