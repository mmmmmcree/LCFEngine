#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_fwd_decls.h"
#include <vma/vk_mem_alloc.h>

namespace lcf::render {
    struct MemoryAllocationCreateInfo;

    class VulkanMemoryAllocator
    {
    public:
        VulkanMemoryAllocator() = default;
        VulkanMemoryAllocator(const VulkanMemoryAllocator &) = delete;
        VulkanMemoryAllocator & operator=(const VulkanMemoryAllocator &) = delete;
        ~VulkanMemoryAllocator();
        bool create(VulkanContext * context);
        VulkanImageUniquePointer createImage(const vk::ImageCreateInfo & image_info, const MemoryAllocationCreateInfo & mem_alloc_info) const;
        VulkanBufferUniquePointer createBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationCreateInfo & mem_alloc_info) const;
    private:
        VmaAllocator m_allocator = nullptr;
    };
}