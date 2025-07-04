#pragma once

#include <functional>
#include <memory>
#include <vulkan/vulkan.hpp>

class VmaAllocator_T;

namespace lcf::render {
    class VulkanContext;

    using VMAUniqueImage = std::unique_ptr<VkImage_T, std::function<void(VkImage)>>;
    using VMAUniqueBuffer = std::unique_ptr<VkBuffer_T, std::function<void(VkBuffer)>>;

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
}