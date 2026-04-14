#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

namespace lcf::render {
    struct MemoryAllocationCreateInfo;

    class VulkanImage;

    class VulkanBuffer;

    class VulkanMemoryAllocator
    {
    public:
        VulkanMemoryAllocator() = default;
        VulkanMemoryAllocator(const VulkanMemoryAllocator &) = delete;
        VulkanMemoryAllocator & operator=(const VulkanMemoryAllocator &) = delete;
        ~VulkanMemoryAllocator();
        std::error_code create(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device) noexcept;
        std::unique_ptr<VulkanImage> createImage(const vk::ImageCreateInfo & image_info, const MemoryAllocationCreateInfo & mem_alloc_info) const;
        std::unique_ptr<VulkanBuffer> createBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationCreateInfo & mem_alloc_info) const;
    private:
        VmaAllocator m_allocator = nullptr;
    };
}