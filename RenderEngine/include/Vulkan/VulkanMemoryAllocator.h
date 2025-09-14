#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>


class VmaAllocator_T;
class VmaAllocation_T;

namespace lcf::render {
    class VulkanContext;

    class VMAImageDeleter;
    class VMABufferDeleter;

    using VMAUniqueImage = std::unique_ptr<VkImage_T, VMAImageDeleter>;
    using VMAUniqueBuffer = std::unique_ptr<VkBuffer_T, VMABufferDeleter>;

    class VulkanMemoryAllocator
    {
    public:
        VulkanMemoryAllocator() = default;
        ~VulkanMemoryAllocator();
        bool create(VulkanContext * context);
        VMAUniqueImage createImage(const vk::ImageCreateInfo & image_info);
        VMAUniqueBuffer createBuffer(const vk::BufferCreateInfo & buffer_info, vk::MemoryPropertyFlags memory_flags, std::byte * & mapped_data);
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