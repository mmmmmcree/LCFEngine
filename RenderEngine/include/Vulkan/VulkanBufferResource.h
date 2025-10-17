#include "common/GPUResource.h"
#include "VulkanMemoryAllocator.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;
    class VulkanMemoryAllocator;

    class VulkanBufferResource : public GPUResource, public STDPointerDefs<VulkanBufferResource>
    {
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<VulkanBufferResource>);
        VulkanBufferResource() = default;
        virtual ~VulkanBufferResource() = default;
        VulkanBufferResource(const VulkanBufferResource &) = delete;
        VulkanBufferResource(VulkanBufferResource && other) : m_buffer_up(std::move(other.m_buffer_up)) {}
        VulkanBufferResource & operator=(const VulkanBufferResource &) = delete;
        VulkanBufferResource & operator=(VulkanBufferResource && other) { m_buffer_up = std::move(other.m_buffer_up); return *this; }
        bool create(VulkanMemoryAllocator & allocator,
            const vk::BufferCreateInfo &buffer_info,
            const MemoryAllocationCreateInfo &memory_allocation_info);
        bool isCreated() const noexcept { return m_buffer_up.get(); }
        vk::Buffer getHandle() const noexcept { return m_buffer_up ? m_buffer_up->getHandle() : nullptr; }
        std::byte * getMappedMemoryPtr() const noexcept { return m_buffer_up->getMappedMemoryPtr(); }
        vk::Result flush(uint32_t offset_in_bytes, uint32_t size_in_bytes) { return m_buffer_up->flush(offset_in_bytes, size_in_bytes); }
    private:
        VMABuffer::UniquePointer m_buffer_up;
    };
}