#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <system_error>
#include <expected>
#include "vk_core/memory/details/Memory.h"

namespace lcf::vkc {

class MemoryAllocationInfo;

class MemoryAllocatorCreateInfo;

} // namespace lcf::vkc

namespace lcf::vkc::details {

class VMAllocator
{
    using Self = VMAllocator;
public:
    ~VMAllocator() noexcept;
    VMAllocator() = default;
    VMAllocator(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    VMAllocator(Self && other) noexcept;
    Self & operator=(Self && other) noexcept;
public:
    std::error_code create(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device, const MemoryAllocatorCreateInfo & create_info) noexcept;
    std::expected<Memory<vk::Buffer>, std::error_code> allocateBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationInfo & alloc_info) const noexcept;
    std::expected<Memory<vk::Image>, std::error_code> allocateImage(const vk::ImageCreateInfo & image_info, const MemoryAllocationInfo & alloc_info) const noexcept;
private:
    VmaAllocator m_allocator = nullptr;
};

} // namespace lcf::vkc::details
