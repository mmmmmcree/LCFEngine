#pragma once

#include "VulkanContext.h"
#include "VulkanShaderProgram.h"
#include "VulkanDescriptorManager.h"

namespace lcf::render {
    class VulkanContext;
    class VulkanDescriptorWriter
    {
    public:
        using DescriptorSetLayoutBindingTable = typename VulkanShaderProgram::DescriptorSetLayoutBindingTable;
        using DescriptorSetList = typename VulkanDescriptorManager::DescriptorSetList;
        using WriteDescriptorSetList = std::vector<vk::WriteDescriptorSet>;
        VulkanDescriptorWriter(VulkanContext * context, const DescriptorSetLayoutBindingTable & binding_table, const DescriptorSetList & descriptor_sets);
        VulkanDescriptorWriter & add(uint32_t set, uint32_t binding, const vk::DescriptorImageInfo & image_info);
        VulkanDescriptorWriter & add(uint32_t set, uint32_t binding, uint32_t index, const vk::DescriptorImageInfo & image_info);
        VulkanDescriptorWriter & add(uint32_t set, uint32_t binding, const vk::DescriptorBufferInfo & buffer_info);
        VulkanDescriptorWriter & add(uint32_t set, uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo & buffer_info);
        void write();
    private:
        VulkanContext * m_context_p;
        const DescriptorSetLayoutBindingTable & m_binding_table;
        const DescriptorSetList & m_descriptor_sets;
        WriteDescriptorSetList m_write_descriptor_sets;
    };
}