#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::render {

    class VulkanDescriptorSetBinding 
    {
        using Self = VulkanDescriptorSetBinding;
    public:
        VulkanDescriptorSetBinding() = default;
        VulkanDescriptorSetBinding(
            uint32_t binding_index,
            vk::DescriptorType descriptor_type,
            uint32_t descriptor_count,
            vk::ShaderStageFlags stage_flags,
            vk::DescriptorBindingFlags flags = {}) :
            m_binding(binding_index, descriptor_type, descriptor_count, stage_flags),
            m_flags(flags)
        {
        }
        VulkanDescriptorSetBinding(const vk::DescriptorSetLayoutBinding & binding, vk::DescriptorBindingFlags flags = {}) :
            m_binding(binding),
            m_flags(flags)
        {
        }
        ~VulkanDescriptorSetBinding() noexcept = default;
        VulkanDescriptorSetBinding(const Self &) = default;
        VulkanDescriptorSetBinding(Self &&) noexcept = default;
        Self & operator=(const Self &) = default;
        Self & operator=(Self &&) noexcept = default;
        Self & addFlags(vk::DescriptorBindingFlags flags) noexcept
        {
            m_flags |= flags;
            return *this;
        }
        bool containsFlags(vk::DescriptorBindingFlagBits flags) const noexcept
        {
            return static_cast<bool>(m_flags & flags);
        }
        Self & setBindingIndex(uint32_t binding) noexcept
        {
            m_binding.setBinding(binding);
            return *this;
        }
        Self & setDescriptorType(vk::DescriptorType type) noexcept
        {
            m_binding.setDescriptorType(type);
            return *this;
        }
        Self & setDescriptorCount(uint32_t count) noexcept
        {
            m_binding.setDescriptorCount(count);
            return *this;
        }
        Self & setStageFlags(vk::ShaderStageFlags flags) noexcept
        {
            m_binding.setStageFlags(flags);
            return *this;
        }
        const uint32_t & getBindingIndex() const noexcept { return m_binding.binding; }
        const vk::DescriptorType & getDescriptorType() const noexcept { return m_binding.descriptorType; }
        const uint32_t & getDescriptorCount() const noexcept { return m_binding.descriptorCount; }
        const vk::ShaderStageFlags & getStageFlags() const noexcept { return m_binding.stageFlags; }
        const vk::DescriptorBindingFlags & getFlags() const noexcept { return m_flags; }
        const vk::DescriptorSetLayoutBinding & getLayoutBinding() const noexcept { return m_binding; }
    private:
        vk::DescriptorBindingFlags m_flags = {};
        vk::DescriptorSetLayoutBinding m_binding = {};
    };

}
