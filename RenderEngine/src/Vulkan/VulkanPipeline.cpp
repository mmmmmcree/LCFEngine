#include "VulkanPipeline.h"
#include "VulkanContext.h"
#include "VulkanShaderProgram.h"
#include "error.h"

using namespace lcf::render;

bool VulkanPipeline::create(VulkanContext *context, const ComputePipelineCreateInfo &create_info)
{
    m_context_p = context;
    m_type = vk::PipelineBindPoint::eCompute;
    m_shader_program = create_info.getShaderProgram();
    if (not m_shader_program or
        not m_shader_program->isLinked() or
        not m_shader_program->containsStage(ShaderTypeFlagBits::Compute)) {
        LCF_THROW_RUNTIME_ERROR("lcf::render::VulkanPipeline::create(): shader program is invalid");
    }
    auto device = m_context_p->getDevice();
    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    pipeline_layout_info.setSetLayouts(m_shader_program->getDescriptorSetLayoutList());
    vk::ComputePipelineCreateInfo compute_pipeline_info;
    compute_pipeline_info.setStage(m_shader_program->getShaderStageInfoList().front())
        .setLayout(m_shader_program->getPipelineLayout());
    auto [create_result, pipeline] = device.createComputePipelineUnique(nullptr, compute_pipeline_info);
    m_pipeline = std::move(pipeline);
    return create_result == vk::Result::eSuccess;
}

bool VulkanPipeline::create(VulkanContext *context, const GraphicPipelineCreateInfo &create_info)
{
    m_context_p = context;
    m_type = vk::PipelineBindPoint::eGraphics;
    m_shader_program = create_info.getShaderProgram();
    if (not m_shader_program or
        not m_shader_program->isLinked() or
        m_shader_program->containsStage(ShaderTypeFlagBits::Compute)) {
        LCF_THROW_RUNTIME_ERROR("lcf::render::VulkanPipeline::create(): shader program is invalid");
    }

    auto device = m_context_p->getDevice();

    vk::PipelineViewportStateCreateInfo viewport_info;
    viewport_info.setScissorCount(1).setViewportCount(1);

    std::vector<vk::DynamicState > dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, };
    vk::PipelineDynamicStateCreateInfo dynamic_info;
    dynamic_info.setDynamicStates(dynamic_states);


    uint32_t vertex_buffer_binding = 0; // 用于cmd.bindVertexBuffers()的bingding参数
    vk::VertexInputBindingDescription vertex_input_binding;
    vertex_input_binding.setInputRate(vk::VertexInputRate::eVertex);
    std::vector<vk::VertexInputAttributeDescription> vertex_input_attributes;
    auto shader = m_shader_program->getShader(ShaderTypeFlagBits::Vertex);
    if (shader) {
        const auto & stage_inputs = shader->getResources().stage_inputs;
        vertex_input_attributes.resize(stage_inputs.size());
        uint32_t offset = 0u;
        for (int i = 0; i < stage_inputs.size(); ++i) {
            const auto & input = stage_inputs[i];
            uint32_t format = static_cast<uint32_t>(input.getBaseDataType()) + input.getVecSize() - 1;
            vertex_input_attributes[i].setBinding(vertex_buffer_binding)
                .setOffset(offset)
                .setFormat(enum_cast<vk::Format>(static_cast<ShaderDataType>(format)))
                .setLocation(input.getLocation());
            offset += input.getSizeInBytes();
        }
        vertex_input_binding.setStride(offset);
    }
    vk::PipelineVertexInputStateCreateInfo vertex_input_info;
    vertex_input_info.setVertexBindingDescriptions(vertex_input_binding)
        .setVertexAttributeDescriptions(vertex_input_attributes);

    vk::PipelineInputAssemblyStateCreateInfo input_assembly_info;
    input_assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

    vk::PipelineRasterizationStateCreateInfo rasterization_info;
    rasterization_info.setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eNone)
        .setFrontFace(vk::FrontFace::eCounterClockwise)
        .setLineWidth(1.0f);

    vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
    depth_stencil_info.setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(false)
        .setStencilTestEnable(false);

    vk::PipelineMultisampleStateCreateInfo multisample_info;
    multisample_info.setRasterizationSamples(create_info.getRasterizationSamples());

    vk::PipelineColorBlendAttachmentState color_blend_attachment_info;
    color_blend_attachment_info.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
        .setBlendEnable(VK_FALSE)
        .setSrcColorBlendFactor(vk::BlendFactor::eOne)
        .setDstColorBlendFactor(vk::BlendFactor::eZero)
        .setColorBlendOp(vk::BlendOp::eAdd)
        .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
        .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
        .setAlphaBlendOp(vk::BlendOp::eAdd);

    vk::PipelineColorBlendStateCreateInfo color_blend_info;
    color_blend_info.setLogicOpEnable(VK_FALSE)
        .setLogicOp(vk::LogicOp::eCopy)
        .setAttachments(color_blend_attachment_info);

    vk::PipelineRenderingCreateInfo rendering_info;
    rendering_info.setColorAttachmentFormats(create_info.getColorAttachmentFormats())
        .setDepthAttachmentFormat(create_info.getDepthAttachmentFormat());
    
    vk::GraphicsPipelineCreateInfo graphics_pipeline_info;
    graphics_pipeline_info.setStages(m_shader_program->getShaderStageInfoList())
        .setPVertexInputState(&vertex_input_info)
        .setPInputAssemblyState(&input_assembly_info)
        .setPRasterizationState(&rasterization_info)
        .setPViewportState(&viewport_info)
        .setPMultisampleState(&multisample_info)
        .setPDepthStencilState(&depth_stencil_info)
        .setPColorBlendState(&color_blend_info)
        .setPDynamicState(&dynamic_info)
        .setLayout(m_shader_program->getPipelineLayout())
        .setPNext(&rendering_info);

    auto [create_result, pipeline] = device.createGraphicsPipelineUnique(nullptr, graphics_pipeline_info);
    m_pipeline = std::move(pipeline);
    return create_result == vk::Result::eSuccess;
}
