#pragma once

#include "shader_core/shader_core_fwd_decls.h"
#include "shader_core/shader_core_enums.h"
#include "vulkan_fwd_decls.h"
#include "ds/VulkanDescriptorSetLayout.h"
#include "VulkanShader.h"
#include "VulkanPushConstant.h"
#include "resource_utils.h"
#include <vector>
#include <map>
#include <span>


namespace lcf::render {
    class VulkanShaderProgram
    {
        using Self = VulkanShaderProgram;
    public:
        using StageToShaderMap = std::unordered_map<ShaderTypeFlagBits, std::shared_ptr<VulkanShader>>;
        using ShaderStageInfoList = std::vector<vk::PipelineShaderStageCreateInfo>;
        using DescriptorSetLayoutMap = std::unordered_map<uint32_t, VulkanDescriptorSetLayout>;
        using DescriptorSetLayoutRefMap = std::map<uint32_t, ResourceRef<const VulkanDescriptorSetLayout>>;
        using PushConstantMap = std::unordered_map<uint32_t, VulkanPushConstant>; // [stage]
        VulkanShaderProgram() = default;
        VulkanShaderProgram(const VulkanShaderProgram &) = delete;
        VulkanShaderProgram & operator=(const VulkanShaderProgram &) = delete;
        ~VulkanShaderProgram();
        Self & addShaderFromGlslFile(ShaderTypeFlagBits stage, std::string_view file_path);
        Self & specifyDescriptorSetLayout(ResourceRef<const VulkanDescriptorSetLayout> layout);
        bool isLinked() const { return m_pipeline_layout.get(); }
        std::error_code link(vk::Device device) noexcept;
        bool containsStage(ShaderTypeFlagBits stage) const { return m_stage_to_shader_map.contains(stage); }
        std::shared_ptr<VulkanShader> getShader(ShaderTypeFlagBits stage) const { return m_stage_to_shader_map.at(stage); }
        bool hasVertexInput() const noexcept;
        const ShaderStageInfoList & getShaderStageInfoList() const { return m_shader_stage_info_list; }
        const VulkanDescriptorSetLayout & getDescriptorSetLayout(uint32_t set_index) const { return *m_descriptor_set_layout_ref_map.at(set_index); }
        const vk::PipelineLayout & getPipelineLayout() const { return m_pipeline_layout.get(); }
        void setPushConstantData(vk::ShaderStageFlags stage, std::span<const void *> data_list);
        void setPushConstantData(vk::ShaderStageFlags stage, const std::initializer_list<const void *> & data_list);
        void bindPushConstants(vk::CommandBuffer cmd);
    private:
        std::error_code createDescriptorSetLayouts(vk::Device device) noexcept;
        std::error_code createPipelineLayout(vk::Device device) noexcept;
    private:
        StageToShaderMap m_stage_to_shader_map;
        ShaderStageInfoList m_shader_stage_info_list;
        DescriptorSetLayoutMap m_descriptor_set_layout_map;
        DescriptorSetLayoutRefMap m_descriptor_set_layout_ref_map;
        PushConstantMap m_push_constant_map;
        vk::UniquePipelineLayout m_pipeline_layout;
    };
}