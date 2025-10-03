#include "VulkanMesh.h"
#include "VulkanContext.h"

bool lcf::render::VulkanMesh::create(VulkanContext *context_p, VulkanCommandBufferObject &cmd, const Mesh & mesh)
{
    if (not context_p or not context_p->isCreated() or not mesh.isCreated() or this->isCreated()) { return false; }
    m_vertex_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .setSize(mesh.getVertexDataSpan().size_bytes())
        .create(context_p);
    m_vertex_buffer.addWriteSegment({mesh.getVertexDataSpan()}).commitWriteSegments(cmd);
    
    m_index_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .setSize(mesh.getIndexDataSpan().size_bytes())
        .create(context_p);
    m_index_buffer.addWriteSegment({mesh.getIndexDataSpan()}).commitWriteSegments(cmd);

    m_vertex_count = mesh.getVertexCount();
    m_vertex_semantic_flags = mesh.getVertexSemanticFlags();
    return this->isCreated();
}