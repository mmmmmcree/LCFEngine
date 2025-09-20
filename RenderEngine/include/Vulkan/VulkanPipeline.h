#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanShaderProgram.h"
#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;
    class VulkanPipeline : public STDPointerDefs<VulkanPipeline>
    {
        using Self = VulkanPipeline;
    public:
        using DescriptorSetLayoutList = VulkanShaderProgram::DescriptorSetLayoutList;
        VulkanPipeline(VulkanContext *context, const VulkanShaderProgram::SharedPointer & shader_program);
        bool create();
        operator bool() const { return this->isValid(); }
        bool isValid() const { return m_pipeline.get(); } 
        Self & setShaderProgram(const VulkanShaderProgram::SharedPointer &shader_program) { m_shader_program = shader_program; return *this; }
        VulkanShaderProgram * getShaderProgram() const { return m_shader_program.get(); }
        vk::PipelineLayout getPipelineLayout() const { return m_shader_program->getPipelineLayout(); }
        vk::Pipeline getHandle() const { return m_pipeline.get(); }
        const DescriptorSetLayoutList & getDescriptorSetLayoutList() const { return m_shader_program->getDescriptorSetLayoutList(); }
        vk::PipelineBindPoint getType() const { return m_type; }
    private:
        bool createComputePipeline();
        bool createGraphicsPipeline();
    private:
        VulkanContext * m_context_p = nullptr;
        vk::PipelineBindPoint m_type;
        vk::UniquePipeline m_pipeline;
        VulkanShaderProgram::SharedPointer m_shader_program;
    };
}