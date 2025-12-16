#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanShaderProgram.h"
#include "VulkanCommandBufferObject.h"
#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;

    class ComputePipelineCreateInfo;
    class GraphicPipelineCreateInfo;

    class VulkanPipeline : public STDPointerDefs<VulkanPipeline>
    {
        using Self = VulkanPipeline;
    public:
        using DescriptorSetLayoutBindings = std::span<const vk::DescriptorSetLayoutBinding>;
        VulkanPipeline() = default;
        VulkanPipeline(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        bool create(VulkanContext * context, const ComputePipelineCreateInfo & create_info);
        bool create(VulkanContext * context, const GraphicPipelineCreateInfo & create_info);
        operator bool() const { return this->isValid(); }
        bool isValid() const { return m_pipeline.get(); } 
        void bind(VulkanCommandBufferObject & cmd) const noexcept;
        void bindDescriptorSet(VulkanCommandBufferObject & cmd, const VulkanDescriptorSet & descriptor_set) const noexcept;
        void bindDescriptorSet(
            VulkanCommandBufferObject & cmd,
            const VulkanDescriptorSet & descriptor_set,
            uint32_t & dynamic_offset) const noexcept;
        Self & setShaderProgram(const VulkanShaderProgram::SharedPointer &shader_program) { m_shader_program = shader_program; return *this; }
        VulkanShaderProgram * getShaderProgram() const { return m_shader_program.get(); }
        vk::PipelineLayout getPipelineLayout() const { return m_shader_program->getPipelineLayout(); }
        vk::Pipeline getHandle() const { return m_pipeline.get(); }
        const VulkanDescriptorSetLayout::SharedPointer & getDescriptorSetLayoutSharedPtr(uint32_t set_index) const { return m_shader_program->getDescriptorSetLayoutSharedPtr(set_index); }
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
            bool depth_test_enable = true,
            bool depth_write_enable = true,
            vk::CompareOp depth_compare_op = vk::CompareOp::eLess,
            vk::SampleCountFlagBits rasterization_samples = vk::SampleCountFlagBits::e1,
            vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eNone,
            vk::FrontFace front_face = vk::FrontFace::eCounterClockwise
        ) : m_shader_program(shader_program),
            m_color_attachment_formats(color_attachment_formats),
            m_depth_attachment_format(depth_attachment_format),
            m_depth_test_enable(depth_test_enable),
            m_depth_write_enable(depth_write_enable),
            m_depth_compare_op(depth_compare_op),
            m_rasterization_samples(rasterization_samples),
            m_cull_mode(cull_mode),
            m_front_face(front_face)
        { }
        Self & setShaderProgram(const VulkanShaderProgram::SharedPointer & shader_program) { m_shader_program = shader_program; return *this; }
        Self & addColorAttachmentFormat(vk::Format format) { m_color_attachment_formats.emplace_back(format); return *this; }
        Self & addColorAttachmentFormats(const std::span<const vk::Format> & formats) { m_color_attachment_formats.insert(m_color_attachment_formats.end(), formats.begin(), formats.end()); return *this; }
        Self & setDepthTestEnabled(bool enable) { m_depth_test_enable = enable; return *this; }
        Self & setDepthWriteEnabled(bool enable) { m_depth_write_enable = enable; return *this; }
        Self & setDepthCompareOp(vk::CompareOp compare_op) { m_depth_compare_op = compare_op; return *this; }
        Self & setDepthAttachmentFormat(vk::Format format) { m_depth_attachment_format = format; return *this; }
        Self & setRasterizationSamples(vk::SampleCountFlagBits samples) { m_rasterization_samples = samples; return *this; }
        Self & setCullMode(vk::CullModeFlags cull_mode) { m_cull_mode = cull_mode; return *this; }
        Self & setFrontFace(vk::FrontFace front_face) { m_front_face = front_face; return *this; }
        const VulkanShaderProgram::SharedPointer & getShaderProgram() const noexcept { return m_shader_program; }
        const std::vector<vk::Format> & getColorAttachmentFormats() const noexcept { return m_color_attachment_formats; }
        bool isDepthTestEnabled() const noexcept { return m_depth_test_enable; }
        bool isDepthWriteEnabled() const noexcept { return m_depth_write_enable; }
        vk::CompareOp getDepthCompareOp() const noexcept { return m_depth_compare_op; }
        vk::Format getDepthAttachmentFormat() const noexcept { return m_depth_attachment_format; }
        vk::SampleCountFlagBits getRasterizationSamples() const noexcept { return m_rasterization_samples; }
        vk::CullModeFlags getCullMode() const noexcept { return m_cull_mode; }
        vk::FrontFace getFrontFace() const noexcept { return m_front_face; }
    private:
        VulkanShaderProgram::SharedPointer m_shader_program;
        std::vector<vk::Format> m_color_attachment_formats;
        vk::Format m_depth_attachment_format;
        bool m_depth_test_enable;
        bool m_depth_write_enable;
        vk::CompareOp m_depth_compare_op;
        vk::SampleCountFlagBits m_rasterization_samples;
        vk::CullModeFlags m_cull_mode;
        vk::FrontFace m_front_face;
    };
}