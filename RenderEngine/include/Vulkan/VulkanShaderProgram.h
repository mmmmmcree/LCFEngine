#pragma once

#include "shader_core/shader_core_fwd_decls.h"
#include "shader_core/shader_core_enums.h"
#include "vulkan_fwd_decls.h"
#include "ds/VulkanDescriptorSetLayout.h"
#include "VulkanShader.h"
#include "VulkanPushConstant.h"
#include <vector>
#include <unordered_map>
#include <span>


namespace lcf::render {
    class VulkanShaderProgram : public STDPointerDefs<VulkanShaderProgram>
    {
        using Self = VulkanShaderProgram;
    public:
        using StageToShaderMap = std::unordered_map<ShaderTypeFlagBits, VulkanShader::SharedPointer>;
        using ShaderStageInfoList = std::vector<vk::PipelineShaderStageCreateInfo>;
        using DescriptorSetLayoutBindingList = std::vector<vk::DescriptorSetLayoutBinding>;
        using DescriptorSetLayoutBindingTable = std::vector<DescriptorSetLayoutBindingList>; // [set][binding]
        using DescriptorSetLayoutSharedPtrList = std::vector<std::shared_ptr<VulkanDescriptorSetLayout>>;
        using PushConstantMap = std::unordered_map<uint32_t, VulkanPushConstant>; // [stage]
        VulkanShaderProgram(VulkanContext * context);
        VulkanShaderProgram(const VulkanShaderProgram &) = delete;
        VulkanShaderProgram & operator=(const VulkanShaderProgram &) = delete;
        ~VulkanShaderProgram();
        Self & addShaderFromGlslFile(ShaderTypeFlagBits stage, std::string_view file_path);
        bool isLinked() const { return m_pipeline_layout.get(); }
        std::error_code link();
        bool containsStage(ShaderTypeFlagBits stage) const { return m_stage_to_shader_map.contains(stage); }
        VulkanShader::SharedPointer getShader(ShaderTypeFlagBits stage) const { return m_stage_to_shader_map.at(stage); }
        bool hasVertexInput() const noexcept;
        const ShaderStageInfoList & getShaderStageInfoList() const { return m_shader_stage_info_list; }
        const VulkanDescriptorSetLayout & getDescriptorSetLayout(uint32_t set_index) const { return *m_descriptor_set_layout_sp_list[set_index]; }
        const vk::PipelineLayout & getPipelineLayout() const { return m_pipeline_layout.get(); }
        const DescriptorSetLayoutBindingList & getDescriptorSetLayoutBindingList(uint32_t set_index) const { return m_descriptor_set_layout_binding_table[set_index]; }
        void setPushConstantData(vk::ShaderStageFlags stage, std::span<const void *> data_list);
        void setPushConstantData(vk::ShaderStageFlags stage, const std::initializer_list<const void *> & data_list);
        void bindPushConstants(vk::CommandBuffer cmd);
    private:
        void createShaderStageInfoList();
        void createDescriptorSetLayoutBindingTable();
        void createDescriptorSetLayouts();
        void createPipelineLayout();
    private:
        VulkanContext * m_context_p = nullptr;
        StageToShaderMap m_stage_to_shader_map;
        ShaderStageInfoList m_shader_stage_info_list;
        DescriptorSetLayoutBindingTable m_descriptor_set_layout_binding_table;
        DescriptorSetLayoutSharedPtrList m_descriptor_set_layout_sp_list;
        PushConstantMap m_push_constant_map;
        vk::UniquePipelineLayout m_pipeline_layout;
    };
}