#include "vk_core/pipeline/graphics/DynamicRendering.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/command/CommandBufferProxy.h"

namespace lcf::vkc {


std::error_code DynamicRendering::create(vk::Device device, const RenderingInfo & info) noexcept
{
    return std::error_code();
}

void DynamicRendering::begin(CommandBufferProxy &cmd, const RenderTarget &target) noexcept
{
    std::vector<vk::RenderingAttachmentInfo> color_attachment_infos;
    vk::RenderingAttachmentInfo depth_stencil_attachment_info;
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(target.getRenderArea())
        .setLayerCount(target.getLayerCount())
        .setColorAttachments(color_attachment_infos)
        .setPDepthAttachment(&depth_stencil_attachment_info);
    cmd.beginRendering(rendering_info);
}

void DynamicRendering::end(CommandBufferProxy &cmd) noexcept
{
    cmd.endRendering();
}

} // namespace lcf::vkc