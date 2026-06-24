#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <cstddef>
#include <span>
#include <utility>
#include <type_traits>

namespace lcf::vkc::details::vma {

inline void destroy_memory(VmaAllocator allocator, vk::Image image, VmaAllocation allocation) noexcept
{
    if (allocator) { vmaDestroyImage(allocator, image, allocation); }
}

inline void destroy_memory(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation allocation) noexcept
{
    if (allocator) { vmaDestroyBuffer(allocator, buffer, allocation); }
}

template <typename Handle>
requires std::is_same_v<Handle, vk::Image> or std::is_same_v<Handle, vk::Buffer>
class Memory
{
    using Self = Memory<Handle>;
    using ByteSpan = std::span<std::byte>;
    using ReadableByteSpan = std::span<const std::byte>;
public:
    ~Memory() noexcept { destroy_memory(m_allocator, m_handle, m_allocation); }
    Memory(VmaAllocator allocator,
        VmaAllocation allocation,
        Handle handle) noexcept :
        m_allocator(allocator),
        m_allocation(allocation),
        m_handle(handle) {}
    Memory(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    Memory(Self && other) noexcept :
        m_allocator(std::exchange(other.m_allocator, nullptr)),
        m_allocation(std::exchange(other.m_allocation, nullptr)),
        m_handle(std::exchange(other.m_handle, nullptr)) {}
    Self & operator=(Self && other) noexcept
    {
        if (this == &other) { return *this; }
        destroy_memory(m_allocator, m_handle, m_allocation);
        m_allocator = std::exchange(other.m_allocator, nullptr);
        m_allocation = std::exchange(other.m_allocation, nullptr);
        m_handle = std::exchange(other.m_handle, nullptr);
        return *this;
    }
public:
    const Handle & getHandle() const noexcept { return m_handle; }
    vk::DeviceSize getSizeInBytes() const noexcept
    {
        VmaAllocationInfo info {};
        vmaGetAllocationInfo(m_allocator, m_allocation, &info);
        return info.size;
    }
    vk::MemoryPropertyFlags getMemoryPropertyFlags() const noexcept
    {
        VkMemoryPropertyFlags flags {};
        vmaGetAllocationMemoryProperties(m_allocator, m_allocation, &flags);
        return flags;
    }
    vk::Result flush(vk::DeviceSize offset_in_bytes = 0, vk::DeviceSize size_in_bytes = vk::WholeSize) const noexcept
    {
        return static_cast<vk::Result>(vmaFlushAllocation(m_allocator, m_allocation, offset_in_bytes, size_in_bytes));
    }
    vk::Result invalidate(vk::DeviceSize offset_in_bytes = 0, vk::DeviceSize size_in_bytes = vk::WholeSize) const noexcept
    {
        return static_cast<vk::Result>(vmaInvalidateAllocation(m_allocator, m_allocation, offset_in_bytes, size_in_bytes));
    }
    vk::Result copyFromMemory(ReadableByteSpan src, vk::DeviceSize offset_in_bytes = 0) noexcept
    {
        auto mapped_mem_span = this->getMappedMemorySpan();
        if (mapped_mem_span.empty()) { return vk::Result::eErrorMemoryMapFailed; }
        std::ranges::copy(src, mapped_mem_span.begin() + offset_in_bytes);
        return vk::Result::eSuccess;
    }
    vk::Result copyToMemory(ByteSpan dst, vk::DeviceSize offset_in_bytes = 0) const noexcept
    {
        auto mapped_mem_span = this->getMappedMemorySpan();
        if (mapped_mem_span.empty()) { return vk::Result::eErrorMemoryMapFailed; }
        std::ranges::copy(mapped_mem_span.begin() + offset_in_bytes, dst);
        return vk::Result::eSuccess;
    }
private:
    ByteSpan getMappedMemorySpan() const noexcept
    {
        VmaAllocationInfo info {};
        vmaGetAllocationInfo(m_allocator, m_allocation, &info);
        std::byte * mapped_mem_p = static_cast<std::byte *>(info.pMappedData);
        if (not mapped_mem_p) { return {}; }
        return { mapped_mem_p, info.size };
    }
private:
    VmaAllocator m_allocator = nullptr;
    VmaAllocation m_allocation = nullptr;
    Handle m_handle = nullptr;
};

} // namespace lcf::vkc::details::vma
