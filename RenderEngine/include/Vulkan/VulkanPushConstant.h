#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <span>

namespace lcf::render {
    class VulkanPushConstant
    {
        using Self = VulkanPushConstant;
    public:
        struct PushConstantData
        {
            uint32_t offset;
            uint32_t size;
            const void * data;
        };
        using PushConstantDataList = std::vector<PushConstantData>;
        VulkanPushConstant() = default;
        void bind(vk::CommandBuffer cmd, vk::PipelineLayout layout) const;
        bool isValid() const { return not m_data_list.empty(); }
        Self & setStageFlags(vk::ShaderStageFlags stage_flags) { m_stage_flags = stage_flags; return *this; }
        vk::ShaderStageFlags getStageFlags() const { return m_stage_flags; }
        Self & setDataLayout(uint32_t index, uint32_t offset, uint32_t size);
        Self & setData(uint32_t index, const void * data);
        Self & setData(std::span<const void *> data_list);
        Self & setData(const std::initializer_list<const void *> &data_list);
        uint32_t getSize() const;
        Self & setOffset(uint32_t offset) { m_offset = offset; return *this; }
        uint32_t getOffset() const { return m_offset; }
    private:
        vk::ShaderStageFlags m_stage_flags = {}; 
        uint32_t m_offset = 0;
        PushConstantDataList m_data_list;
    };
}