#include "common/GPUResource.h"
#include "VulkanMemoryAllocator.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;
    class VulkanMemoryAllocator;

    class VulkanBufferResource : public GPUResource, public PointerDefs<VulkanBufferResource>
    {
    public:
        IMPORT_POINTER_DEFS(VulkanBufferResource);
        VulkanBufferResource() = default;
        bool create(VulkanMemoryAllocator * allocator_p,
            const vk::BufferCreateInfo &buffer_info,
            const MemoryAllocationCreateInfo &memory_allocation_info);
        bool isCreated() const noexcept { return m_buffer_up.get(); }
        vk::Buffer getHandle() const noexcept { return m_buffer_up->getHandle(); }
        std::byte * getMappedMemoryPtr() const noexcept { return m_buffer_up->getMappedMemoryPtr(); }
        vk::Result flush(uint32_t offset_in_bytes, uint32_t size_in_bytes) { return m_buffer_up->flush(offset_in_bytes, size_in_bytes); }
    private:
        VMABuffer::UniquePointer m_buffer_up;
    };
}