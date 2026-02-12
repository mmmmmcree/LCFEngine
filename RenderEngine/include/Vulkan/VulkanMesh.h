#pragma once

#include "render_assets/render_assets_fwd_decls.h"
#include "VulkanBufferObject.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanCommandBufferObject;

    class VulkanMesh
    {
    public:
        VulkanMesh() = default;
        bool isCreated() const { return m_vertex_buffer.isCreated() and m_index_buffer.isCreated(); }
        bool create(VulkanContext * context_p, VulkanCommandBufferObject & cmd, const Geometry & geometry);
        const vk::DeviceAddress & getVertexBufferAddress() const noexcept { return m_vertex_buffer.getDeviceAddress(); }
        const vk::DeviceAddress & getIndexBufferAddress() const noexcept { return m_index_buffer.getDeviceAddress(); }
        uint32_t getVertexCount() const noexcept { return m_vertex_count; }
        uint32_t getIndexCount() const noexcept { return m_index_count; }
    private:
        VulkanBufferObject m_vertex_buffer;
        VulkanBufferObject m_index_buffer;
        uint32_t m_vertex_count;
        uint32_t m_index_count;
    };
}