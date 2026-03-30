#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_enums.h"
#include "VulkanDescriptorSetBinding.h"
#include "VulkanDescriptorSetUpdater2.h"
#include <vector>

namespace lcf::render {

    class PerFramePoolManager;
    class BindlessPoolManager;
    class PersistentPoolManager;

    class VulkanDescriptorSet2
    {
        using Self = VulkanDescriptorSet2;
    public:
        using BindingList = std::vector<VulkanDescriptorSetBinding>;

        VulkanDescriptorSet2() = default;
        ~VulkanDescriptorSet2() = default;
        VulkanDescriptorSet2(const Self &) = default;
        Self & operator=(const Self &) = default;
        VulkanDescriptorSet2(Self &&) = default;
        Self & operator=(Self &&) = default;

        const vk::DescriptorSet & getHandle() const noexcept { return m_descriptor_set; }
        uint32_t getIndex() const noexcept { return m_set_index; }
        vkenums::DescriptorSetStrategy getStrategy() const noexcept { return m_strategy; }
        std::span<const VulkanDescriptorSetBinding> getBindings() const noexcept { return m_binding_list; }

        VulkanDescriptorSetUpdater2 createUpdater(vk::Device device) const noexcept;

    private:
        friend class PerFramePoolManager;
        friend class BindlessPoolManager;
        friend class PersistentPoolManager;

        vk::DescriptorSet m_descriptor_set;
        BindingList m_binding_list;
        vkenums::DescriptorSetStrategy m_strategy = vkenums::DescriptorSetStrategy::ePerFrame;
        uint32_t m_set_index = 0u;
    };

}
