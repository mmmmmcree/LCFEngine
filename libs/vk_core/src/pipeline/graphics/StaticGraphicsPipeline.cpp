#include "vk_core/pipeline/graphics/StaticGraphicsPipeline.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

std::error_code StaticGraphicsPipeline::create(vk::Device device, const GraphicsPipelineInfo &pipeline_info) noexcept
{
    const ShaderProgramInfo & shader_program_info = pipeline_info.getShaderProgramInfo();
    auto shader_stage_infos_view = shader_program_info.viewStageInfos();
    std::size_t shader_stage_count = shader_stage_infos_view.size();
    std::vector<vk::PipelineShaderStageCreateInfo> pipeline_shader_stage_infos;
    std::vector<vk::PushConstantRange> push_constant_ranges;
    m_shader_modules.reserve(shader_stage_infos_view.size());
    pipeline_shader_stage_infos.reserve(shader_stage_count);
    for (const auto & stage_info : shader_stage_infos_view) {
        vk::ShaderModuleCreateInfo shader_module_create_info;
        shader_module_create_info.setCode(stage_info.getCode());
        try {
            m_shader_modules.emplace_back(device.createShaderModuleUnique(shader_module_create_info));
        } catch (const vk::SystemError & e) {
            return e.code();
        } 
        vk::PipelineShaderStageCreateInfo pipeline_shader_stage_info;
        pipeline_shader_stage_info.setStage(stage_info.getStage())
            .setModule(m_shader_modules.back().get())
            .setPName(stage_info.getEntryPoint().c_str())
            .setPSpecializationInfo(&stage_info.getSpecializationInfo());
        pipeline_shader_stage_infos.emplace_back(pipeline_shader_stage_info);
        push_constant_ranges.append_range(stage_info.getPushConstantRanges());
    } 
    auto descriptor_set_layouts = shader_program_info.viewDescriptorSetLayouts() | stdr::to<std::vector>();
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info;
    pipeline_layout_create_info.setPushConstantRanges(push_constant_ranges)
        .setSetLayouts(descriptor_set_layouts);
    try {
        m_pipeline_layout = device.createPipelineLayoutUnique(pipeline_layout_create_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    vk::GraphicsPipelineCreateInfo pipeline_create_info;
    pipeline_create_info.setFlags(pipeline_info.getFlags())
        .setStages(pipeline_shader_stage_infos)
        .setPVertexInputState(&static_cast<const vk::PipelineVertexInputStateCreateInfo &>(pipeline_info.getVertexInputInfo()))
        .setPInputAssemblyState(&static_cast<const vk::PipelineInputAssemblyStateCreateInfo &>(pipeline_info.getInputAssemblyStateInfo()))
        .setPTessellationState(&static_cast<const vk::PipelineTessellationStateCreateInfo &>(pipeline_info.getTessellationStateInfo()))
        .setPViewportState(&static_cast<const vk::PipelineViewportStateCreateInfo &>(pipeline_info.getViewportStateInfo()))
        .setPRasterizationState(&static_cast<const vk::PipelineRasterizationStateCreateInfo &>(pipeline_info.getRasterizationStateInfo()))
        .setPMultisampleState(&static_cast<const vk::PipelineMultisampleStateCreateInfo &>(pipeline_info.getMultisampleStateInfo()))
        .setPDepthStencilState(&static_cast<const vk::PipelineDepthStencilStateCreateInfo &>(pipeline_info.getDepthStencilStateInfo()))
        .setPColorBlendState(&static_cast<const vk::PipelineColorBlendStateCreateInfo &>(pipeline_info.getColorBlendStateInfo()))
        .setPDynamicState(&static_cast<const vk::PipelineDynamicStateCreateInfo &>(pipeline_info.getDynamicStateInfo()))
        .setLayout(m_pipeline_layout.get());
    //     .setRenderPass()
    //     .setSubpass();
    try {
        auto [result, pipeline] = device.createGraphicsPipelineUnique(nullptr, pipeline_create_info);
        if (result != vk::Result::eSuccess) { return result; }
        m_pipeline = std::move(pipeline);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

void StaticGraphicsPipeline::bind(CommandBufferProxy & cmd) const noexcept
{
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
}

} // namespace lcf::vkc
