#pragma once

#include "vulkan_fwd_decls.h"
#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

namespace lcf::render {
    struct MemoryAllocationCreateInfo
    {
        MemoryAllocationCreateInfo(vk::MemoryPropertyFlags _memory_flags) :
            memory_flags(_memory_flags) {}
        vk::MemoryPropertyFlags memory_flags;
    };

    class VulkanImage : public VulkanImagePointerDefs
    {
        using Self = VulkanImage;
    public:
        VulkanImage(VmaAllocator allocator,
            VmaAllocation allocation,
            vk::Image image,
            vk::DeviceSize size,
            void * mapped_data_p = nullptr);
        explicit VulkanImage(vk::Image image); //- native vk image wrapper for swapchain
        ~VulkanImage() noexcept;
        VulkanImage(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanImage(Self &&) noexcept;
        Self & operator=(Self &&) noexcept;
        const vk::Image & getHandle() const noexcept { return m_image; }
        std::byte * getMappedMemoryPtr() const noexcept { return m_mapped_data_p; }
        vk::DeviceSize getSize() const noexcept { return m_size; }
        vk::Result flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    private:
        VmaAllocator m_allocator = nullptr;
        VmaAllocation m_allocation = nullptr;
        vk::Image m_image = nullptr;
        std::byte * m_mapped_data_p = nullptr;
        vk::DeviceSize m_size = 0;
    };

    class VulkanBuffer : public VulkanBufferPointerDefs
    {
        using Self = VulkanBuffer;
    public:
        VulkanBuffer(VmaAllocator allocator,
            VmaAllocation allocation,
            vk::Buffer buffer,
            vk::DeviceSize size,
            void * mapped_data_p = nullptr);
        ~VulkanBuffer() noexcept;
        VulkanBuffer(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanBuffer(Self &&) noexcept;
        Self & operator=(Self &&) noexcept;
        const vk::Buffer & getHandle() const noexcept { return m_buffer; }
        std::byte * getMappedMemoryPtr() const noexcept { return m_mapped_data_p; }
        vk::DeviceSize getSize() const noexcept { return m_size; }
        vk::Result flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    private:
        VmaAllocator m_allocator = nullptr;
        VmaAllocation m_allocation = nullptr;
        vk::Buffer m_buffer = nullptr;
        std::byte * m_mapped_data_p = nullptr;
        vk::DeviceSize m_size = 0;
    };
}