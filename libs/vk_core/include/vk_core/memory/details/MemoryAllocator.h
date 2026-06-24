#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <system_error>
#include <expected>
#include "vk_core/memory/details/Memory.h"

namespace lcf::vkc {

class MemoryAllocationInfo;

} // namespace lcf::vkc

namespace lcf::vkc::details {

class MemoryAllocatorCreateInfo;

class MemoryAllocator
{
    using Self = MemoryAllocator;
public:
    ~MemoryAllocator() noexcept;
    MemoryAllocator() = default;
    MemoryAllocator(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    MemoryAllocator(Self && other) noexcept;
    Self & operator=(Self && other) noexcept;
public:
    std::error_code create(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device, const MemoryAllocatorCreateInfo & create_info) noexcept;
    std::expected<Memory<vk::Buffer>, std::error_code> allocateBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationInfo & alloc_info) const noexcept;
    std::expected<Memory<vk::Image>, std::error_code> allocateImage(const vk::ImageCreateInfo & image_info, const MemoryAllocationInfo & alloc_info) const noexcept;
    bool isBufferDeviceAddressEnabled() const noexcept { return m_buffer_device_address_enabled; }
private:
    VmaAllocator m_allocator = nullptr;
    bool m_buffer_device_address_enabled = false;
};

} // namespace lcf::vkc::details
