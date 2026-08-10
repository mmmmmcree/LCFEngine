#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <cstddef>
#include <span>
#include <utility>
#include <type_traits>

namespace lcf::vkc::details {

namespace vma {

inline void destroy_memory(VmaAllocator allocator, vk::Image image, VmaAllocation allocation) noexcept
{
    if (allocator) { vmaDestroyImage(allocator, image, allocation); }
}

inline void destroy_memory(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation allocation) noexcept
{
    if (allocator) { vmaDestroyBuffer(allocator, buffer, allocation); }
}

} // namespace lcf::vkc::details::vma

template <typename Handle>
requires std::is_same_v<Handle, vk::Image> or std::is_same_v<Handle, vk::Buffer>
class Memory
{
    using Self = Memory<Handle>;
    using ByteSpan = std::span<std::byte>;
    using ReadableByteSpan = std::span<const std::byte>;
public:
    ~Memory() noexcept = default;
    Memory() noexcept = default;
    Memory(VmaAllocator allocator,
        VmaAllocation allocation,
        Handle handle) noexcept :
        m_allocator(allocator),
        m_allocation(allocation),
        m_handle(handle) {}
    Memory(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Memory(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    Memory(std::nullptr_t) noexcept {}
    Self & operator=(std::nullptr_t) noexcept { *this = Self {}; return *this; }
    bool operator==(const Self &) const noexcept = default;
    explicit operator bool() const noexcept { return static_cast<bool>(m_handle); }
    operator const Handle &() const noexcept { return m_handle; }
public:
    void destroy() const noexcept { vma::destroy_memory(m_allocator, m_handle, m_allocation); }
    const Handle & handle() const noexcept { return m_handle; }
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
        return static_cast<vk::MemoryPropertyFlags>(flags);
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

template <typename Dispatch>
class MemoryDestroy
{
public:
    MemoryDestroy() = default;
    MemoryDestroy(Dispatch const &) noexcept {}
protected:
    template <typename T>
    void destroy(T t) const noexcept { t.destroy(); }
};

} // namespace lcf::vkc::details

namespace VULKAN_HPP_NAMESPACE {

template <>
struct isVulkanHandleType<lcf::vkc::details::Memory<vk::Buffer>>
{
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = true;
};

template <>
struct isVulkanHandleType<lcf::vkc::details::Memory<vk::Image>>
{
    static VULKAN_HPP_CONST_OR_CONSTEXPR bool value = true;
};

template <typename Dispatch>
class UniqueHandleTraits<lcf::vkc::details::Memory<vk::Buffer>, Dispatch>
{
public:
    using deleter = lcf::vkc::details::MemoryDestroy<Dispatch>;
};

template <typename Dispatch>
class UniqueHandleTraits<lcf::vkc::details::Memory<vk::Image>, Dispatch>
{
public:
    using deleter = lcf::vkc::details::MemoryDestroy<Dispatch>;
};

} // namespace VULKAN_HPP_NAMESPACE

namespace lcf::vkc::details {

using UniqueBufferMemory = vk::UniqueHandle<Memory<vk::Buffer>, vk::detail::DispatchLoaderDynamic>;
using UniqueImageMemory = vk::UniqueHandle<Memory<vk::Image>, vk::detail::DispatchLoaderDynamic>;

} // namespace lcf::vkc::details
