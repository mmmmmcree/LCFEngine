#pragma once

#include "PointerDefs.h"
#include "vulkan_memory_resources.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanMemoryAllocator
    {
    public:
        VulkanMemoryAllocator() = default;
        VulkanMemoryAllocator(const VulkanMemoryAllocator &) = delete;
        VulkanMemoryAllocator & operator=(const VulkanMemoryAllocator &) = delete;
        ~VulkanMemoryAllocator();
        bool create(VulkanContext * context);
        VulkanImage::UniquePointer createImage(const vk::ImageCreateInfo & image_info, const MemoryAllocationCreateInfo & mem_alloc_info) const;
        VulkanBuffer::UniquePointer createBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationCreateInfo & mem_alloc_info) const;
    private:
        VmaAllocator m_allocator = nullptr;
    };
}