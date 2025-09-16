#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"


class VmaAllocator_T;
class VmaAllocation_T;

namespace lcf::render {
    class VulkanContext;

    class VMAImageDeleter;
    class VMABufferDeleter;

    using VMAUniqueImage = std::unique_ptr<VkImage_T, VMAImageDeleter>;
    using VMAUniqueBuffer = std::unique_ptr<VkBuffer_T, VMABufferDeleter>;

    class VMABuffer : public PointerDefs<VMABuffer>
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
        VMAUniqueImage createImage(const vk::ImageCreateInfo & image_info);
        VMAUniqueBuffer createBuffer(const vk::BufferCreateInfo & buffer_info, vk::MemoryPropertyFlags memory_flags, std::byte * & mapped_data);

        VMABuffer::UniquePointer createBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationCreateInfo & mem_alloc_info);
    private:
        VmaAllocator_T * m_allocator = nullptr;
    };

    class VMABufferDeleter
    {
    public:
        VMABufferDeleter() = default;
        VMABufferDeleter(VmaAllocator_T * allocator, VmaAllocation_T * allocation);
        void operator()(VkBuffer buffer) const noexcept;
    private:
        VmaAllocator_T * m_allocator = nullptr;
        VmaAllocation_T * m_allocation = nullptr;
    };

    class VMAImageDeleter
    {
    public:
        VMAImageDeleter() = default;
        VMAImageDeleter(VmaAllocator_T * allocator, VmaAllocation_T * allocation);
        void operator()(VkImage image) const noexcept;
    private:
        VmaAllocator_T * m_allocator = nullptr;
        VmaAllocation_T * m_allocation = nullptr;
    };
}