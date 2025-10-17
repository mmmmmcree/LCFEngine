#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <span>
#include "VulkanDescriptorWriter.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanDescriptorSetPrototype  
    {
    public:
        using BindingListView = std::span<const vk::DescriptorSetLayoutBinding>;
        VulkanDescriptorSetPrototype(BindingListView bindings, uint32_t set = 0) :
            m_set(set), m_bindings(bindings.begin(), bindings.end()) {}
        void create(VulkanContext * context_p);
        uint32_t getSet() const noexcept { return m_set; }
        vk::DescriptorSetLayout getLayout() const noexcept { return m_layout.get(); }
        const BindingListView & getBindings() const noexcept { return m_bindings; }
        VulkanDescriptorWriter generateWriter() const noexcept { return VulkanDescriptorWriter(m_context_p, m_bindings); }
    private:
        VulkanContext * m_context_p = nullptr;
        uint32_t m_set;
        vk::UniqueDescriptorSetLayout m_layout;
        BindingListView m_bindings;
    };
}