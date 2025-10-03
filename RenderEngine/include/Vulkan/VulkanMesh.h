#pragma once

#include "Mesh.h"
#include "VulkanBufferObject.h"
#include "VulkanCommandBufferObject.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanMesh
    {
    public:
        VulkanMesh() = default;
        bool isCreated() const { return m_vertex_buffer.isCreated() and m_index_buffer.isCreated(); }
        bool create(VulkanContext * context_p, VulkanCommandBufferObject & cmd, const Mesh & mesh);
        const vk::DeviceAddress & getVertexBufferAddress() const noexcept { return m_vertex_buffer.getDeviceAddress(); }
        const vk::DeviceAddress & getIndexBufferAddress() const noexcept { return m_index_buffer.getDeviceAddress(); }
    private:
        VulkanBufferObject m_vertex_buffer;
        VulkanBufferObject m_index_buffer;
        Mesh::FaceBuffer m_face_buffer;
        uint32_t m_vertex_count;
        uint32_t m_index_count;
        VertexSemanticFlags m_vertex_semantic_flags;
    };
}