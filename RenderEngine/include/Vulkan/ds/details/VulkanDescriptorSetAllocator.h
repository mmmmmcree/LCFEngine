#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_enums.h"
#include <vector>
#include <expected>
#include <tsl/robin_map.h>

namespace lcf::render {

    class VulkanDescriptorSet;
    class VulkanDescriptorSetLayout;

namespace detail {

    class VulkanDescriptorSetAllocator
    {
        using Self = VulkanDescriptorSetAllocator;
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
        using AllocResult = std::expected<VulkanDescriptorSet, std::error_code>;
    public:
        VulkanDescriptorSetAllocator() = default;
        ~VulkanDescriptorSetAllocator() noexcept;
        VulkanDescriptorSetAllocator(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetAllocator(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        std::error_code create(vk::Device device) noexcept;
        AllocResult allocate(const VulkanDescriptorSetLayout & layout) noexcept;
        void deallocate(VulkanDescriptorSet && set);
    private:
        AllocResult allocate(const VulkanDescriptorSetLayout & layout, const vk::DescriptorSetAllocateInfo & alloc_info) noexcept;
        vk::DescriptorPool tryGetPool(vkenums::DescriptorSetStrategy strategy) noexcept;
        vk::DescriptorPool createPool(vkenums::DescriptorSetStrategy strategy) noexcept;
    private:
        vk::Device m_device = nullptr;
        PoolGroupMap m_pool_groups;
        SetToPoolMap m_set_to_pool_map;
    };

} // namespace detail

}
