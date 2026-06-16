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
    if (allocator and allocation) {
        vmaDestroyImage(allocator, image, allocation);
    }
}

inline void destroy_memory(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation allocation) noexcept
{
    if (allocator and allocation) {
        vmaDestroyBuffer(allocator, buffer, allocation);
    }
}


template <typename Bundle>
requires std::is_same_v<Bundle, vk::Image> || std::is_same_v<Bundle, vk::Buffer>
class Memory
{
    using Self = Memory<Bundle>;
    using ByteSpan = std::span<std::byte>;
    using ReadableByteSpan = std::span<const std::byte>;
public:
    ~Memory() noexcept { destroy_memory(m_allocator, m_bundle, m_allocation); }
    Memory(VmaAllocator allocator,
        VmaAllocation allocation,
        Bundle bundle,
        vk::DeviceSize size) noexcept :
        m_allocator(allocator),
        m_allocation(allocation),
        m_bundle(bundle),
        m_size(size) {}
    explicit Memory(Bundle bundle) noexcept : m_bundle(bundle) {}
    Memory(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    Memory(Self && other) noexcept :
        m_allocator(std::exchange(other.m_allocator, nullptr)),
        m_allocation(std::exchange(other.m_allocation, nullptr)),
        m_bundle(std::exchange(other.m_bundle, nullptr)),
        m_size(std::exchange(other.m_size, 0)) {}
    Self & operator=(Self && other) noexcept
    {
        if (this == &other) { return *this; }
        destroy_memory(m_allocator, m_bundle, m_allocation);
        m_allocator = std::exchange(other.m_allocator, nullptr);
        m_allocation = std::exchange(other.m_allocation, nullptr);
        m_bundle = std::exchange(other.m_bundle, nullptr);
        m_size = std::exchange(other.m_size, 0);
        return *this;
    }
public:
    const Bundle & getBundle() const noexcept { return m_bundle; }
    const vk::DeviceSize & getSize() const noexcept { return m_size; }
    vk::MemoryPropertyFlags getMemoryPropertyFlags() const noexcept
    {
        VkMemoryPropertyFlags flags {};
        vmaGetAllocationMemoryProperties(m_allocator, m_allocation, &flags);
        return flags;
    }
    vk::Result flush(vk::DeviceSize offset = 0, vk::DeviceSize size = vk::WholeSize) const noexcept
    {
        return static_cast<vk::Result>(vmaFlushAllocation(m_allocator, m_allocation, offset, size));
    }
    vk::Result invalidate(vk::DeviceSize offset = 0, vk::DeviceSize size = vk::WholeSize) const noexcept
    {
        return static_cast<vk::Result>(vmaInvalidateAllocation(m_allocator, m_allocation, offset, size));
    }
    vk::Result copyFromMemory(ReadableByteSpan src, vk::DeviceSize offset = 0) noexcept
    {
        return static_cast<vk::Result>(vmaCopyMemoryToAllocation(m_allocator, src.data(), m_allocation, offset, src.size()));
    }
    vk::Result copyToMemory(ByteSpan dst, vk::DeviceSize offset = 0) const noexcept
    {
        return static_cast<vk::Result>(vmaCopyAllocationToMemory(m_allocator, m_allocation, offset, dst.data(), dst.size()));
    }
private:
    VmaAllocator m_allocator = nullptr;
    VmaAllocation m_allocation = nullptr;
    Bundle m_bundle = nullptr;
    vk::DeviceSize m_size = 0;
};

} // namespace lcf::vkc::details::vma
