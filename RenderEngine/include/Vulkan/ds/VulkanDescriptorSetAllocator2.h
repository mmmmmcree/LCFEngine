#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_enums.h"
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>

namespace lcf::render {

    class VulkanContext;
    class VulkanDescriptorSet2;
    class VulkanDescriptorSetLayout2;

    class VulkanDescriptorSetAllocator2
    {
        using Self = VulkanDescriptorSetAllocator2;
    public:
        struct PoolGroup
        {
            vk::DescriptorPool getCurrentAvailablePool() const noexcept;
            void setCurrentAvailablePoolFull();
            void resetAllocatedSets(vk::Device device);
            void destroyPools(vk::Device device);

            std::vector<vk::DescriptorPool> m_full_pools;
            std::vector<vk::DescriptorPool> m_available_pools;
        };

        VulkanDescriptorSetAllocator2() = default;
        ~VulkanDescriptorSetAllocator2();
        VulkanDescriptorSetAllocator2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetAllocator2(Self &&) = default;
        Self & operator=(Self &&) = default;

        void create(VulkanContext * context_p);

        // Allocate ePerFrame / ePersistent descriptor set
        VulkanDescriptorSet2 allocate(const VulkanDescriptorSetLayout2 & layout);

        // Allocate eBindless descriptor set (maxSets=1, eUpdateAfterBind pool).
        // Pool is created internally and tracked by handle — caller never sees it.
        VulkanDescriptorSet2 allocateBindless(const VulkanDescriptorSetLayout2 & layout,
                                              uint32_t variable_count);

        // Deallocate (ePerFrame = no-op, ePersistent = individual free)
        void deallocate(vk::DescriptorSet handle, vkenums::DescriptorSetStrategy strategy);

        // Deallocate a bindless set — destroys its backing pool via internal mapping
        void deallocateBindless(vk::DescriptorSet handle);

        // Batch reset pools for a given strategy (caller decides when)
        void resetPools(vkenums::DescriptorSetStrategy strategy);

    private:
        vk::DescriptorPool acquirePool(vkenums::DescriptorSetStrategy strategy);
        vk::DescriptorPool createPool(vkenums::DescriptorSetStrategy strategy);
        PoolGroup & getPoolGroup(vkenums::DescriptorSetStrategy strategy);

        VulkanContext * m_context_p = nullptr;
        // [0] = ePerFrame, [1] = ePersistent
        mutable std::array<PoolGroup, 2> m_pool_groups;
        // Bindless: each DS has its own pool (maxSets=1), tracked by DS handle
        std::unordered_map<VkDescriptorSet, vk::DescriptorPool> m_bindless_pool_map;
    };

}
