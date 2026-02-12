#include "Vulkan/VulkanMesh.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "common/glsl_type_traits.h"
#include "render_assets/Geometry.h"

using namespace lcf::render;

bool VulkanMesh::create(VulkanContext *context_p, VulkanCommandBufferObject &cmd, const Geometry & geometry)
{
    if (not context_p or not context_p->isCreated() or this->isCreated()) { return false; }
    auto vertex_data_segments = geometry.generateInterleavedVertexBufferSegments<glsl::std140::enum_value_type_mapping_t>();
    m_vertex_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .create(context_p, vertex_data_segments.getUpperBoundInBytes());
    for (const auto &segment : vertex_data_segments) {
        m_vertex_buffer.addWriteSegment(segment);
    }
    auto indices_bytes = as_bytes(geometry.getIndices());
    m_index_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .create(context_p, indices_bytes.size());
    m_vertex_buffer.commit(cmd);
    m_index_buffer.addWriteSegment(indices_bytes).commit(cmd);
    m_vertex_count = geometry.getVertexCount();
    m_index_count = geometry.getIndexCount();
    return this->isCreated();
}
