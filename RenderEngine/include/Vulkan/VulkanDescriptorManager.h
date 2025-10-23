#pragma once

#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"
#include <vector>
#include <optional>
#include <span>
#include <unordered_map>

namespace lcf::render {
    class VulkanContext;

    class VulkanDescriptorManager
    {
    public:
        using DescriptorPoolList = std::vector<vk::DescriptorPool>;
        using DescriptorSetList = std::vector<vk::DescriptorSet>;
        using UniqueDescriptorSetList = std::vector<vk::UniqueDescriptorSet>;
        struct PoolGroup
        {
            vk::DescriptorPool getCurrentAvailablePool() const noexcept;
            void setCurrentAvailablePoolFull();
            void destroyPools(vk::Device device);
            DescriptorPoolList m_full_pools;
            DescriptorPoolList m_available_pools;
        };
        using PoolGroupMap = std::unordered_map<uint32_t, PoolGroup>;
        VulkanDescriptorManager() = default;
        ~VulkanDescriptorManager();
        void create(VulkanContext * context_p);
        vk::DescriptorSet allocate(vk::DescriptorSetLayout layout);
        DescriptorSetList allocate(std::span<const vk::DescriptorSetLayout> layouts);
        vk::UniqueDescriptorSet allocateUnique(vk::DescriptorSetLayout layout);
        UniqueDescriptorSetList allocateUnique(std::span<const vk::DescriptorSetLayout> layouts);
        void resetAllocatedSets();
    private:
        vk::DescriptorPool createNewPool(vk::DescriptorPoolCreateFlags flags); //todo may pass CreateInfo instead
        vk::DescriptorPool tryGetPool(vk::DescriptorPoolCreateFlags flags);
        PoolGroup & getPoolGroup(vk::DescriptorPoolCreateFlags flags);
    private:
        VulkanContext * m_context_p = nullptr;
        PoolGroupMap m_pool_group_map;
    };
}