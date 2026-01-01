#pragma once

#include "common/GPUResource.h"
#include "VulkanMemoryAllocator.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;
    class VulkanMemoryAllocator;

    class VulkanBuffer : public GPUResource, public STDPointerDefs<VulkanBuffer>
    {
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<VulkanBuffer>);
        VulkanBuffer() = default;
        virtual ~VulkanBuffer() = default;
        VulkanBuffer(const VulkanBuffer &) = delete;
        VulkanBuffer(VulkanBuffer && other) : m_buffer_up(std::move(other.m_buffer_up)) {}
        VulkanBuffer & operator=(const VulkanBuffer &) = delete;
        VulkanBuffer & operator=(VulkanBuffer && other) { m_buffer_up = std::move(other.m_buffer_up); return *this; }
        bool create(const VulkanMemoryAllocator & allocator,
            const vk::BufferCreateInfo &buffer_info,
            const MemoryAllocationCreateInfo &memory_allocation_info);
        bool isCreated() const noexcept { return m_buffer_up.get(); }
        vk::Buffer getHandle() const noexcept { return m_buffer_up ? m_buffer_up->getHandle() : nullptr; }
        std::byte * getMappedMemoryPtr() const noexcept { return m_buffer_up->getMappedMemoryPtr(); }
        vk::Result flush(uint32_t offset_in_bytes, uint32_t size_in_bytes) { return m_buffer_up->flush(offset_in_bytes, size_in_bytes); }
        vk::DeviceSize getSizeInBytes() const noexcept { return m_buffer_up->getSize(); }
    private:
        VMABuffer::UniquePointer m_buffer_up;
    };
}