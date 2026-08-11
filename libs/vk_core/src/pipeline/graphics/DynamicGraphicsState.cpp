#include "vk_core/pipeline/graphics/DynamicGraphicsState.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <vector>

namespace lcf::vkc {

void bind_dynamic_graphics_state(
    CommandBufferProxy & cmd,
    const GraphicsPipelineInfo & info,
    const ShaderObjectBindingState & shader_object_binding_states) noexcept
{
    const auto & vertex_input = info.getVertexInputInfo();
    std::vector<vk::VertexInputBindingDescription2EXT> bindings;
    bindings.reserve(vertex_input.getBindings().size());
    for (const auto & binding : vertex_input.getBindings()) {
        bindings.emplace_back(static_cast<const vk::VertexInputBindingDescription2EXT &>(binding));
    }
    std::vector<vk::VertexInputAttributeDescription2EXT> attributes;
    attributes.reserve(vertex_input.getAttributes().size());
    for (const auto & attribute : vertex_input.getAttributes()) {
        attributes.emplace_back(static_cast<const vk::VertexInputAttributeDescription2EXT &>(attribute));
    }
    cmd.setVertexInputEXT(bindings, attributes);

    const auto & input_assembly = info.getInputAssemblyStateInfo();
    cmd.setPrimitiveTopologyEXT(input_assembly.getTopology());
    cmd.setPrimitiveRestartEnableEXT(input_assembly.isPrimitiveRestartEnabled());

    if (input_assembly.getTopology() == vk::PrimitiveTopology::ePatchList) {
        const auto & tessellation = info.getTessellationStateInfo();
        cmd.setPatchControlPointsEXT(tessellation.getPatchControlPoints());
    }

    const auto & viewport = info.getViewportStateInfo();
    if (not viewport.getViewports().empty()) {
        cmd.setViewportWithCountEXT(viewport.getViewports());
    }
    if (not viewport.getScissors().empty()) {
        cmd.setScissorWithCountEXT(viewport.getScissors());
    }

    const auto & rasterization = info.getRasterizationStateInfo();
    cmd.setDepthClampEnableEXT(rasterization.isDepthClampEnabled());
    cmd.setRasterizerDiscardEnableEXT(rasterization.isRasterizerDiscardEnabled());
    cmd.setPolygonModeEXT(rasterization.getPolygonMode());
    cmd.setCullModeEXT(rasterization.getCullMode());
    cmd.setFrontFaceEXT(rasterization.getFrontFace());
    cmd.setDepthBiasEnableEXT(rasterization.isDepthBiasEnabled());
    cmd.setDepthBias(
        rasterization.getDepthBiasConstantFactor(),
        rasterization.getDepthBiasClamp(),
        rasterization.getDepthBiasSlopeFactor());
    cmd.setLineWidth(rasterization.getLineWidth());

    const auto & multisample = info.getMultisampleStateInfo();
    const auto samples = multisample.getRasterizationSamples();
    cmd.setRasterizationSamplesEXT(samples);
    if (multisample.getSampleMask().empty()) {
        const auto sample_count = static_cast<uint32_t>(samples);
        std::vector<vk::SampleMask> sample_mask((sample_count + 31u) / 32u, ~vk::SampleMask {0});
        cmd.setSampleMaskEXT(samples, sample_mask);
    } else {
        cmd.setSampleMaskEXT(samples, multisample.getSampleMask());
    }
    cmd.setAlphaToCoverageEnableEXT(multisample.isAlphaToCoverageEnabled());
    cmd.setAlphaToOneEnableEXT(multisample.isAlphaToOneEnabled());

    const auto & depth_stencil = info.getDepthStencilStateInfo();
    cmd.setDepthTestEnableEXT(depth_stencil.isDepthTestEnabled());
    cmd.setDepthWriteEnableEXT(depth_stencil.isDepthWriteEnabled());
    cmd.setDepthCompareOpEXT(depth_stencil.getDepthCompareOp());
    cmd.setDepthBoundsTestEnableEXT(depth_stencil.isDepthBoundsTestEnabled());
    cmd.setDepthBounds(depth_stencil.getMinDepthBounds(), depth_stencil.getMaxDepthBounds());
    cmd.setStencilTestEnableEXT(depth_stencil.isStencilTestEnabled());

    const auto & front_stencil = depth_stencil.getFrontStencilState();
    const auto & back_stencil = depth_stencil.getBackStencilState();
    cmd.setStencilOpEXT(
        vk::StencilFaceFlagBits::eFront,
        front_stencil.failOp,
        front_stencil.passOp,
        front_stencil.depthFailOp,
        front_stencil.compareOp);
    cmd.setStencilOpEXT(
        vk::StencilFaceFlagBits::eBack,
        back_stencil.failOp,
        back_stencil.passOp,
        back_stencil.depthFailOp,
        back_stencil.compareOp);
    cmd.setStencilCompareMask(vk::StencilFaceFlagBits::eFront, front_stencil.compareMask);
    cmd.setStencilCompareMask(vk::StencilFaceFlagBits::eBack, back_stencil.compareMask);
    cmd.setStencilWriteMask(vk::StencilFaceFlagBits::eFront, front_stencil.writeMask);
    cmd.setStencilWriteMask(vk::StencilFaceFlagBits::eBack, back_stencil.writeMask);
    cmd.setStencilReference(vk::StencilFaceFlagBits::eFront, front_stencil.reference);
    cmd.setStencilReference(vk::StencilFaceFlagBits::eBack, back_stencil.reference);

    const auto & color_blend = info.getColorBlendStateInfo();
    cmd.setLogicOpEnableEXT(color_blend.isLogicOpEnabled());
    cmd.setLogicOpEXT(color_blend.getLogicOp());
    cmd.setBlendConstants(color_blend.getBlendConstants().data());

    const auto & attachments = color_blend.getColorBlendAttachmentStates();
    if (not attachments.empty()) {
        std::vector<vk::Bool32> blend_enables;
        std::vector<vk::ColorBlendEquationEXT> blend_equations;
        std::vector<vk::ColorComponentFlags> write_masks;
        blend_enables.reserve(attachments.size());
        blend_equations.reserve(attachments.size());
        write_masks.reserve(attachments.size());
        for (const auto & attachment : attachments) {
            blend_enables.emplace_back(attachment.blendEnable);
            blend_equations.emplace_back(
                attachment.srcColorBlendFactor,
                attachment.dstColorBlendFactor,
                attachment.colorBlendOp,
                attachment.srcAlphaBlendFactor,
                attachment.dstAlphaBlendFactor,
                attachment.alphaBlendOp);
            write_masks.emplace_back(attachment.colorWriteMask);
        }
        cmd.setColorBlendEnableEXT(0u, blend_enables);
        cmd.setColorBlendEquationEXT(0u, blend_equations);
        cmd.setColorWriteMaskEXT(0u, write_masks);
    }

    shader_object_binding_states.bind(cmd);
}

} // namespace lcf::vkc
