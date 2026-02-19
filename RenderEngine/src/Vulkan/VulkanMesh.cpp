#include "Vulkan/VulkanMesh.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "common/glsl_type_traits.h"
#include "render_assets/Geometry.h"

using namespace lcf::render;
namespace stdr = std::ranges;

std::error_code VulkanMesh::create(VulkanContext *context_p, VulkanCommandBufferObject &cmd, const BufferWriteSegments &vertex_data_segments, std::span<const uint32_t> indices)
{
    if (this->isCreated()) { return std::make_error_code(std::errc::operation_not_permitted); }
    m_vertex_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .create(context_p, vertex_data_segments.getUpperBoundInBytes());
    m_index_buffer.setUsage(GPUBufferUsage::eShaderStorage)
        .setPattern(GPUBufferPattern::eStatic)
        .create(context_p, indices.size_bytes());
    for (const auto & segment : vertex_data_segments) {
        m_vertex_buffer.addWriteSegment(segment);
    }
    m_indices.append_range(indices);
    m_vertex_buffer.commit(cmd);
    m_index_buffer.addWriteSegment({as_bytes(m_indices)}).commit(cmd);
    m_vertex_count = stdr::max(indices) + 1;
    m_index_count = indices.size();
    return {};
}
