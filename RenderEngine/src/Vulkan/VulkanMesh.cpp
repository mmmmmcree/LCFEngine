#include "Vulkan/VulkanMesh.h"
#include "Vulkan/VulkanContext.h"
#include "common/glsl_alignment_traits.h"

bool lcf::render::VulkanMesh::create(VulkanContext *context_p, VulkanCommandBufferObject &cmd, const Mesh &mesh)
{
    if (not context_p or not context_p->isCreated() or not mesh.isCreated() or this->isCreated()) { return false; }
    auto vertex_data_segments = mesh.generateInterleavedVertexBufferSegments<glsl::std140::ShaderTypeMapping>();
    m_vertex_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .setSize(vertex_data_segments.getUpperBoundInBytes())
        .create(context_p);
    for (const auto &segment : vertex_data_segments) {
        m_vertex_buffer.addWriteSegment(segment);
    }
    m_index_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .setSize(mesh.getIndexDataSpan().size_bytes())
        .create(context_p);
    m_vertex_buffer.commitWriteSegments(cmd);
    m_index_buffer.addWriteSegment({mesh.getIndexDataSpan()}).commitWriteSegments(cmd);
    m_vertex_count = mesh.getVertexCount();
    m_index_count = mesh.getIndexCount();
    m_vertex_semantic_flags = mesh.getVertexSemanticFlags();
    return this->isCreated();
}
