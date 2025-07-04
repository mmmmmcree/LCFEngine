#pragma once

#include "RHI/GPUBuffer.h"
#include "VulkanMemoryAllocator.h"
#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;
    class VulkanBuffer : public GPUBuffer, public PointerDefs<VulkanBuffer>
    {
    public:
        VulkanBuffer(VulkanContext * context);
        ~VulkanBuffer() = default;
        virtual bool create() override;
        virtual void resize(uint32_t size_in_bytes) override;
        virtual void setData(const void *data, uint32_t size_in_bytes) override;
        virtual void setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes) override;
        virtual bool isCreated() const override { return m_buffer.get(); }
        void copyFrom(const VulkanBuffer &src, uint32_t data_size_in_bytes = 0u, uint32_t src_offset_in_bytes = 0u, uint32_t dst_offset_in_bytes = 0u);
        void setUsageFlags(vk::BufferUsageFlags flags);
        vk::BufferUsageFlags getUsageFlags() const { return m_usage_flags; }
        bool hasUsageFlagsBits(vk::BufferUsageFlagBits usage_flag) const { return static_cast<bool>(m_usage_flags & usage_flag); }
        void setSharingMode(vk::SharingMode sharing_mode) { m_sharing_mode = sharing_mode; }
        vk::Buffer getHandle() const { return m_buffer.get(); }
        vk::DeviceAddress getDeviceAddress() const;
    private:
        void setSubDataByMap(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes);
        void setSubDataByStagingBuffer(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes);
    private:
        VulkanContext * m_context = nullptr;
        VMAUniqueBuffer m_buffer;
        std::byte *m_mapped_memory_ptr = nullptr;
        vk::BufferUsageFlags m_usage_flags;
        vk::SharingMode m_sharing_mode = vk::SharingMode::eExclusive;
    };
}