#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanDescriptorSet2.h"
#include <vector>

namespace lcf::render {

    class VulkanContext;
    class VulkanDescriptorSetLayout2;

    class PersistentPoolManager
    {
        using Self = PersistentPoolManager;
    public:
        PersistentPoolManager() = default;
        ~PersistentPoolManager();
        PersistentPoolManager(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        PersistentPoolManager(Self &&) = default;
        Self & operator=(Self &&) = default;

        void create(VulkanContext * context_p);
        VulkanDescriptorSet2 allocate(const VulkanDescriptorSetLayout2 & layout);
        void free(vk::DescriptorSet descriptor_set);

    private:
        vk::DescriptorPool acquirePool();
        vk::DescriptorPool createPool();

        VulkanContext * m_context_p = nullptr;
        std::vector<vk::DescriptorPool> m_available_pools;
        std::vector<vk::DescriptorPool> m_full_pools;
    };

}
