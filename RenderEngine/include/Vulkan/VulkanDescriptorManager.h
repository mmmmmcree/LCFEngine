#pragma once

#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"
#include <vector>
#include <optional>
#include <span>

namespace lcf::render {
    class VulkanContext;
    class VulkanDescriptorManager
    {
    public:
        using DescriptorPoolList = std::vector<vk::DescriptorPool>;
        using DescriptorSetList = std::vector<vk::DescriptorSet>;
        VulkanDescriptorManager() = default;
        ~VulkanDescriptorManager();
        void create(VulkanContext * context);
        std::optional<vk::DescriptorSet> allocate(const vk::DescriptorSetLayout &layout);
        std::optional<DescriptorSetList> allocate(std::span<const vk::DescriptorSetLayout> layouts);
        void resetAllocatedSets();
    private:
        void createNewPool();
    private:
        VulkanContext * m_context_p = nullptr;
        DescriptorPoolList m_full_pools;
        DescriptorPoolList m_available_pools;
    };
}