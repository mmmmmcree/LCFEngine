#include "Vulkan/VulkanMesh.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "common/glsl_alignment_traits.h"

using namespace lcf::render;

bool VulkanMesh::create(VulkanContext *context_p, VulkanCommandBufferObject &cmd, const Mesh &mesh)
{
    if (not context_p or not context_p->isCreated() or not mesh.isCreated() or this->isCreated()) { return false; }
    auto vertex_data_segments = mesh.generateInterleavedVertexBufferSegments<glsl::std140::ShaderTypeMapping>();
    m_vertex_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .create(context_p, vertex_data_segments.getUpperBoundInBytes());
    for (const auto &segment : vertex_data_segments) {
        m_vertex_buffer.addWriteSegment(segment);
    }
    m_index_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .create(context_p, mesh.getIndexDataSpan().size_bytes());
    m_vertex_buffer.commit(cmd);
    m_index_buffer.addWriteSegment({mesh.getIndexDataSpan()}).commit(cmd);
    m_vertex_count = mesh.getVertexCount();
    m_index_count = mesh.getIndexCount();
    m_vertex_semantic_flags = mesh.getVertexSemanticFlags();
    return this->isCreated();
}
