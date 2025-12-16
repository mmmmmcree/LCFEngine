#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanDescriptorSet.h"
#include <vector>
#include <unordered_map>

namespace lcf::render {
    class VulkanContext;

    class VulkanDescriptorSetAllocator
    {
        using Self = VulkanDescriptorSetAllocator;
    public:
        using DescriptorPoolList = std::vector<vk::DescriptorPool>;
        struct PoolGroup
        {
            vk::DescriptorPool getCurrentAvailablePool() const noexcept;
            void setCurrentAvailablePoolFull();
            void resetAllocatedSets(VulkanContext * context_p);
            void destroyPools(VulkanContext * context_p);
            DescriptorPoolList m_full_pools;
            DescriptorPoolList m_available_pools;
        };
        using PoolGroupMap = std::unordered_map<uint32_t, PoolGroup>;
        VulkanDescriptorSetAllocator() = default;
        ~VulkanDescriptorSetAllocator();
        VulkanDescriptorSetAllocator(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetAllocator(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        void create(VulkanContext * context_p);
        void allocate(VulkanDescriptorSet & descriptor_set) const;
        void resetAllocatedSets(vk::DescriptorPoolCreateFlags pool_flags = {}) const; //? probably useless
    private:
        vk::DescriptorPool tryGetPool(vk::DescriptorPoolCreateFlags flags) const;
        vk::DescriptorPool createNewPool(vk::DescriptorPoolCreateFlags flags) const; //todo may pass CreateInfo instead
        PoolGroup & getPoolGroup(vk::DescriptorPoolCreateFlags flags) const;
    private:
        VulkanContext * m_context_p = nullptr;
        mutable PoolGroupMap m_pool_group_map;
    };
}