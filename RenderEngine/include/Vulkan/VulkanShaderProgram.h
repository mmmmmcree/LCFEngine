#pragma once

#include "ShaderCore/ShaderProgram.h"
#include "VulkanShader.h"
#include "VulkanPushConstant.h"
#include <vector>
#include <span>


namespace lcf::render {
    class VulkanContext;
    class VulkanShaderProgram : public ShaderProgram, public PointerDefs<VulkanShaderProgram>
    {
    public:
        IMPORT_POINTER_DEFS(VulkanShaderProgram);
        using ShaderStageInfoList = std::vector<vk::PipelineShaderStageCreateInfo>;
        using DescriptorSetLayoutBindingTable = std::vector<std::vector<vk::DescriptorSetLayoutBinding>>; // [set][binding]
        using DescriptorSetLayoutList = std::vector<vk::DescriptorSetLayout>;
        using PushConstantMap = std::unordered_map<uint32_t, VulkanPushConstant>; // [stage]
        VulkanShaderProgram(VulkanContext * context);
        ~VulkanShaderProgram();
        void addShaderFromGlslFile(ShaderTypeFlagBits stage, std::string_view file_path);
        virtual bool link() override;
        const ShaderStageInfoList & getShaderStageInfoList() const { return m_shader_stage_info_list; }
        const DescriptorSetLayoutList & getDescriptorSetLayoutList() const { return m_descriptor_set_layout_list; }
        const DescriptorSetLayoutBindingTable & getDescriptorSetLayoutBindingTable() const { return m_descriptor_set_layout_binding_table; }
        vk::PipelineLayout getPipelineLayout() const { return m_pipeline_layout.get(); }
        void setPushConstantData(vk::ShaderStageFlags stage, std::span<const void *> data_list);
        void setPushConstantData(vk::ShaderStageFlags stage, const std::initializer_list<const void *> & data_list);
        void bindPushConstants(vk::CommandBuffer cmd);
    private:
        void createShaderStageInfoList();
        void createDescriptorSetLayoutBindingTable();
        void createDescriptorSetLayoutList();
        void createPipelineLayout();
    private:
        VulkanContext * m_context_p = nullptr;
        ShaderStageInfoList m_shader_stage_info_list;
        DescriptorSetLayoutBindingTable m_descriptor_set_layout_binding_table;
        DescriptorSetLayoutList m_descriptor_set_layout_list;
        PushConstantMap m_push_constant_map;
        vk::UniquePipelineLayout m_pipeline_layout;
    };
}