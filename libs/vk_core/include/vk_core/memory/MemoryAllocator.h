#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>
#include <expected>
#include "vk_core/memory/details/Allocator.h"
#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/Image.h"

namespace lcf::vkc {

class MemoryAllocationInfo;

class MemoryAllocatorCreateInfo;

class MemoryAllocator
{
    using Self = MemoryAllocator;
public:
    ~MemoryAllocator() noexcept = default;
    MemoryAllocator() = default;
    MemoryAllocator(const Self &) = delete;
    MemoryAllocator(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    std::error_code create(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device, const MemoryAllocatorCreateInfo & create_info) noexcept;
    const vk::Device & getDevice() const noexcept { return m_device; }
    std::expected<Buffer, std::error_code> createBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationInfo & alloc_info) const noexcept;
    std::expected<Image, std::error_code> createImage(const vk::ImageCreateInfo & image_info, const MemoryAllocationInfo & alloc_info) const noexcept;
private:
    vk::Device m_device;
    details::VMAllocator m_allocator;
    bool m_bda_enabled = false;
};

} // namespace lcf::vkc