#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanShaderProgram.h"
#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;

    class ComputePipelineCreateInfo;
    class GraphicPipelineCreateInfo;

    class VulkanPipeline : public STDPointerDefs<VulkanPipeline>
    {
        using Self = VulkanPipeline;
    public:
        using DescriptorSetLayoutList = VulkanShaderProgram::DescriptorSetLayoutList;
        using DescriptorSetLayoutBindings = std::span<const vk::DescriptorSetLayoutBinding>;
        VulkanPipeline() = default;
        bool create(VulkanContext * context, const ComputePipelineCreateInfo & create_info);
        bool create(VulkanContext * context, const GraphicPipelineCreateInfo & create_info);
        operator bool() const { return this->isValid(); }
        bool isValid() const { return m_pipeline.get(); } 
        Self & setShaderProgram(const VulkanShaderProgram::SharedPointer &shader_program) { m_shader_program = shader_program; return *this; }
        VulkanShaderProgram * getShaderProgram() const { return m_shader_program.get(); }
        vk::PipelineLayout getPipelineLayout() const { return m_shader_program->getPipelineLayout(); }
        vk::Pipeline getHandle() const { return m_pipeline.get(); }
        const DescriptorSetLayoutList & getDescriptorSetLayoutList() const { return m_shader_program->getDescriptorSetLayoutList(); }
        vk::DescriptorSetLayout getDescriptorSetLayout(uint32_t set) const noexcept { return m_shader_program->getDescriptorSetLayoutList()[set]; }
        DescriptorSetLayoutBindings getDescriptorSetLayoutBindings(uint32_t set) const noexcept { return m_shader_program->getDescriptorSetLayoutBindingTable()[set]; }
        vk::PipelineBindPoint getType() const { return m_type; }
    private:
        VulkanContext * m_context_p = nullptr;
        vk::PipelineBindPoint m_type;
        vk::UniquePipeline m_pipeline;
        VulkanShaderProgram::SharedPointer m_shader_program;
    };

    class ComputePipelineCreateInfo
    {
        using Self = ComputePipelineCreateInfo;
    public:
        ComputePipelineCreateInfo(const VulkanShaderProgram::SharedPointer & shader_program = nullptr) : m_shader_program(shader_program) { }
        Self & setShaderProgram(const VulkanShaderProgram::SharedPointer & shader_program) { m_shader_program = shader_program; return *this; }
        const VulkanShaderProgram::SharedPointer & getShaderProgram() const { return m_shader_program; }
    private:
        VulkanShaderProgram::SharedPointer m_shader_program;
    };

    class GraphicPipelineCreateInfo
    {
        using Self = GraphicPipelineCreateInfo;
    public:
        GraphicPipelineCreateInfo(
            const VulkanShaderProgram::SharedPointer & shader_program = nullptr,
            uint32_t scissor_count = 1,
            vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList,
            const std::vector<vk::Format> & color_attachment_formats = {},
            vk::Format depth_attachment_format = vk::Format::eUndefined,
            vk::SampleCountFlagBits rasterization_samples = vk::SampleCountFlagBits::e1
        ) : m_shader_program(shader_program),
            m_color_attachment_formats(color_attachment_formats),
            m_depth_attachment_format(depth_attachment_format),
            m_rasterization_samples(rasterization_samples) { }
        Self & setShaderProgram(const VulkanShaderProgram::SharedPointer & shader_program) { m_shader_program = shader_program; return *this; }
        Self & addColorAttachmentFormat(vk::Format format) { m_color_attachment_formats.emplace_back(format); return *this; }
        Self & addColorAttachmentFormats(const std::span<const vk::Format> & formats) { m_color_attachment_formats.insert(m_color_attachment_formats.end(), formats.begin(), formats.end()); return *this; }
        Self & setDepthAttachmentFormat(vk::Format format) { m_depth_attachment_format = format; return *this; }
        Self & setRasterizationSamples(vk::SampleCountFlagBits samples) { m_rasterization_samples = samples; return *this; }
        const VulkanShaderProgram::SharedPointer & getShaderProgram() const { return m_shader_program; }
        const std::vector<vk::Format> & getColorAttachmentFormats() const { return m_color_attachment_formats; }
        vk::Format getDepthAttachmentFormat() const { return m_depth_attachment_format; }
        vk::SampleCountFlagBits getRasterizationSamples() const { return m_rasterization_samples; }
    private:
        VulkanShaderProgram::SharedPointer m_shader_program;
        std::vector<vk::Format> m_color_attachment_formats;
        vk::Format m_depth_attachment_format;
        vk::SampleCountFlagBits m_rasterization_samples;
    };
}