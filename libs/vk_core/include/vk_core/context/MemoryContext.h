#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>
#include <expected>
#include "vk_core/memory/details/MemoryAllocator.h"
#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/Image.h"

namespace lcf::vkc {

class MemoryAllocationInfo;

namespace details {

class MemoryAllocatorCreateInfo;

}

class MemoryContext
{
    using Self = MemoryContext;
public:
    ~MemoryContext() noexcept = default;
    MemoryContext() = default;
    MemoryContext(const Self &) = delete;
    MemoryContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    std::error_code create(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device, const details::MemoryAllocatorCreateInfo & create_info) noexcept;
    const vk::Device & getDevice() const noexcept { return m_device; }
    std::expected<Buffer, std::error_code> createBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationInfo & alloc_info) const noexcept;
    std::expected<Image, std::error_code> createImage(const vk::ImageCreateInfo & image_info, const MemoryAllocationInfo & alloc_info) const noexcept;
private:
    vk::Device m_device;
    details::MemoryAllocator m_allocator;
};

} // namespace lcf::vkc