#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_enums.h"
#include <vector>
#include <expected>
#include <tsl/robin_map.h>

namespace lcf::render {

    class VulkanDescriptorSet2;
    class VulkanDescriptorSetLayout2;

namespace detail {

    class VulkanDescriptorSetAllocator2
    {
        using Self = VulkanDescriptorSetAllocator2;
        struct PoolGroup
        {
            PoolGroup() = default;

            vk::DescriptorPool getCurrentAvailablePool() const noexcept;
            void setCurrentAvailablePoolFull();
            void destroyCurrentAvailablePool(vk::Device device);
            void destroyPools(vk::Device device) noexcept;
            void addPool(vk::DescriptorPool pool);

            std::vector<vk::DescriptorPool> m_full_pools;
            std::vector<vk::DescriptorPool> m_available_pools;
        };
        using PoolGroupMap = tsl::robin_map<vkenums::DescriptorSetStrategy, PoolGroup>;
        using SetToPoolMap = std::unordered_map<vkenums::DescriptorSetStrategy, std::unordered_map<VkDescriptorSet, vk::DescriptorPool>>;
    public:
        using AllocResult = std::expected<VulkanDescriptorSet2, std::error_code>;
    public:
        VulkanDescriptorSetAllocator2() = default;
        ~VulkanDescriptorSetAllocator2() noexcept;
        VulkanDescriptorSetAllocator2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetAllocator2(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        std::error_code create(vk::Device device) noexcept;
        AllocResult allocate(const VulkanDescriptorSetLayout2 & layout) noexcept;
        void deallocate(VulkanDescriptorSet2 && set);
    private:
        AllocResult allocate(const VulkanDescriptorSetLayout2 & layout, const vk::DescriptorSetAllocateInfo & alloc_info) noexcept;
        vk::DescriptorPool tryGetPool(vkenums::DescriptorSetStrategy strategy) noexcept;
        vk::DescriptorPool createPool(vkenums::DescriptorSetStrategy strategy) noexcept;
    private:
        vk::Device m_device = nullptr;
        PoolGroupMap m_pool_groups;
        SetToPoolMap m_set_to_pool_map;
    };

} // namespace detail

}
