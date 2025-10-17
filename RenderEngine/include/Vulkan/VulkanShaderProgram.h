#pragma once

#include "ShaderCore/ShaderProgram.h"
#include "VulkanDescriptorSetPrototype.h"
#include "VulkanShader.h"
#include "VulkanPushConstant.h"
#include <vector>
#include <span>


namespace lcf::render {
    class VulkanContext;
    class VulkanShaderProgram : public ShaderProgram, public STDPointerDefs<VulkanShaderProgram>
    {
        using Self = VulkanShaderProgram;
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<VulkanShaderProgram>);
        using ShaderStageInfoList = std::vector<vk::PipelineShaderStageCreateInfo>;
        using DescriptorSetLayoutBindingList = std::vector<vk::DescriptorSetLayoutBinding>;
        using DescriptorSetLayoutBindingTable = std::vector<DescriptorSetLayoutBindingList>; // [set][binding]
        using DescriptorSetLayoutList = std::vector<vk::DescriptorSetLayout>;
        using DescriptorSetPrototypeList = std::vector<VulkanDescriptorSetPrototype>;
        using PushConstantMap = std::unordered_map<uint32_t, VulkanPushConstant>; // [stage]
        VulkanShaderProgram(VulkanContext * context);
        VulkanShaderProgram(const VulkanShaderProgram &) = delete;
        VulkanShaderProgram & operator=(const VulkanShaderProgram &) = delete;
        ~VulkanShaderProgram();
        Self & addShaderFromGlslFile(ShaderTypeFlagBits stage, std::string_view file_path);
        virtual bool link() override;
        bool hasVertexInput() const noexcept;
        const ShaderStageInfoList & getShaderStageInfoList() const { return m_shader_stage_info_list; }
        const VulkanDescriptorSetPrototype & getDescriptorSetPrototype(uint32_t set_index) const { return m_descriptor_set_prototype_list[set_index]; }
        vk::PipelineLayout getPipelineLayout() const { return m_pipeline_layout.get(); }
        const DescriptorSetLayoutBindingList & getDescriptorSetLayoutBindingList(uint32_t set_index) const { return m_descriptor_set_layout_binding_table[set_index]; }
        void setPushConstantData(vk::ShaderStageFlags stage, std::span<const void *> data_list);
        void setPushConstantData(vk::ShaderStageFlags stage, const std::initializer_list<const void *> & data_list);
        void bindPushConstants(vk::CommandBuffer cmd);
    private:
        void createShaderStageInfoList();
        void createDescriptorSetLayoutBindingTable();
        void createDescriptorSetPrototypes();
        void createPipelineLayout();
    private:
        VulkanContext * m_context_p = nullptr;
        ShaderStageInfoList m_shader_stage_info_list;
        DescriptorSetLayoutBindingTable m_descriptor_set_layout_binding_table;
        DescriptorSetPrototypeList m_descriptor_set_prototype_list;
        PushConstantMap m_push_constant_map;
        vk::UniquePipelineLayout m_pipeline_layout;
    };
}