#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"


class VmaAllocator_T;
class VmaAllocation_T;

namespace lcf::render {
    class VulkanContext;

    class VMAImage : public STDPointerDefs<VMAImage>
    {
    public:
        VMAImage(VmaAllocator_T * allocator,
            VmaAllocation_T * allocation,
            vk::Image image,
            vk::DeviceSize size);
        ~VMAImage();
        vk::Image getHandle() const noexcept { return m_image; }
        vk::DeviceSize getSize() const noexcept { return m_size; }
    private:
        VmaAllocator_T * m_allocator = nullptr;
        VmaAllocation_T * m_allocation = nullptr;
        vk::Image m_image = nullptr;
        vk::DeviceSize m_size = 0;
    };

    class VMABuffer : public STDPointerDefs<VMABuffer>
    {
        using Self = VMABuffer;
    public:
        VMABuffer(VmaAllocator_T * allocator,
            VmaAllocation_T * allocation,
            vk::Buffer buffer,
            vk::DeviceSize size,
            void * mapped_data_p = nullptr);
        ~VMABuffer();
        vk::Buffer getHandle() const noexcept { return m_buffer; }
        std::byte * getMappedMemoryPtr() const noexcept { return m_mapped_data_p; }
        vk::DeviceSize getSize() const noexcept { return m_size; }
        vk::Result flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    private:
        VmaAllocator_T * m_allocator = nullptr;
        VmaAllocation_T * m_allocation = nullptr;
        vk::Buffer m_buffer = nullptr;
        std::byte * m_mapped_data_p = nullptr;
        vk::DeviceSize m_size = 0;
    };

    struct MemoryAllocationCreateInfo
    {
        MemoryAllocationCreateInfo(vk::MemoryPropertyFlags _memory_flags) :
            memory_flags(_memory_flags) {}
        vk::MemoryPropertyFlags memory_flags;
    };

    class VulkanMemoryAllocator
    {
    public:
        VulkanMemoryAllocator() = default;
        ~VulkanMemoryAllocator();
        bool create(VulkanContext * context);
        VMAImage::UniquePointer createImage(const vk::ImageCreateInfo & image_info);
        VMABuffer::UniquePointer createBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationCreateInfo & mem_alloc_info);
    private:
        VmaAllocator_T * m_allocator = nullptr;
    };
}