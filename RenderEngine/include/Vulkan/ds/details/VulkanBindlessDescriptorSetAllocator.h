#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_enums.h"
#include <vector>
#include <expected>
#include <unordered_map>

namespace lcf::render {

    class VulkanDescriptorSet2;
    class VulkanDescriptorSetLayout2;

namespace detail {
    class VulkanBindlessDescriptorSetAllocator
    {
        using Self = VulkanBindlessDescriptorSetAllocator;
        using SetToPoolMap = std::unordered_map<VkDescriptorSet, vk::DescriptorPool>;
    public:
        using AllocResult = std::expected<VulkanDescriptorSet2, std::error_code>;
    public:
        VulkanBindlessDescriptorSetAllocator() = default;
        ~VulkanBindlessDescriptorSetAllocator() noexcept;
        VulkanBindlessDescriptorSetAllocator(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanBindlessDescriptorSetAllocator(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        std::error_code create(vk::Device device) noexcept;
        AllocResult allocate(const VulkanDescriptorSetLayout2 & layout, uint32_t variable_count) noexcept;
        void deallocate(VulkanDescriptorSet2 && set) noexcept;
    private:
        vk::DescriptorPool createPool(const VulkanDescriptorSetLayout2 & layout, uint32_t variable_count) noexcept;
    private:
        vk::Device m_device = nullptr;
        SetToPoolMap m_set_to_pool_map;
    };
} // namespace detail
}