#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>
#include <span>
#include <type_traits>
#include "resource_utils.h"
#include "vk_core/utils/ResourceHandle.h"
#include "vk_core/memory/details/Memory.h"

namespace lcf::vkc {

class MemoryAllocator;

class MemoryAllocationInfo;

class Buffer
{
    using Self = Buffer;
    using Memory = details::Memory<vk::Buffer>;
    using ResourceHandle = utils::ResourceHandle<Memory>;
public:
    ~Buffer() noexcept = default;
    Buffer() = default;
    Buffer(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Buffer(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    operator const vk::Buffer &() const noexcept { return this->handle(); }
public:
    std::error_code create(
        const MemoryAllocator & allocator,
        const vk::BufferCreateInfo & buffer_info,
        const MemoryAllocationInfo & alloc_info) noexcept;
    const vk::Buffer & handle() const noexcept { return m_memory_rh->handle(); }
    ResourceLease lease() const noexcept { return m_memory_rh.lease(); }
    const vk::DeviceSize & getSizeInBytes() const noexcept { return m_size; }
    const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_device_address; }
    std::span<std::byte> getMappedMemorySpan() const noexcept;
    std::error_code copyFromMemory(std::span<const std::byte> src, vk::DeviceSize offset_in_bytes = 0) noexcept;
    std::error_code flush(vk::DeviceSize offset = 0u, vk::DeviceSize size = vk::WholeSize) const noexcept;
private:
    ResourceHandle m_memory_rh;
    vk::DeviceAddress m_device_address = 0u;
    vk::DeviceSize m_size = 0u;
};

} // namespace lcf::vkc
