#include "Vulkan/VulkanBufferWriter.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/VulkanTimelineSemaphore.h"
#include "Vulkan/VulkanBufferProxy.h"
#include "Vulkan/vulkan_utililtie.h"

using namespace lcf::render;

VulkanBufferWriter::~VulkanBufferWriter() noexcept = default;

bool VulkanBufferWriter::create(VulkanContext *context_p)
{
    m_context_p = context_p;
    m_timeline_semaphore_up = VulkanTimelineSemaphore::makeUnique();
    return m_timeline_semaphore_up->create(m_context_p);
}

bool lcf::render::VulkanBufferWriter::hasPendingOperations() const noexcept
{
    return not m_timeline_semaphore_up->isTargetReached();
}

VulkanBufferWriter & VulkanBufferWriter::addWriteRequest(
    VulkanBufferProxy &buffer_proxy,
    const BufferWriteSegments &segments) noexcept
{
    m_write_requests.emplace_back(std::make_pair(&buffer_proxy, &segments));
    return *this;
}

void VulkanBufferWriter::write(VulkanCommandBufferObject &cmd) const noexcept
{
    if (m_write_requests.empty()) { return; }
    using ExecuteWriteSequenceMethod = void (Self::*)(VulkanCommandBufferObject &, VulkanBufferProxy &, const BufferWriteSegments &) const;
    ExecuteWriteSequenceMethod execute_write_sequence_method = &Self::executeGpuWriteSequence;
    if (m_pattern == GPUBufferPattern::eDynamic and m_timeline_semaphore_up->isTargetReached()) {
        execute_write_sequence_method = &Self::executeCpuWriteSequence;
    }
    for (auto [buffer_proxy_p, segments_p] : m_write_requests) {
        (this->*execute_write_sequence_method)(cmd, *buffer_proxy_p, *segments_p);
        cmd.acquireResource(buffer_proxy_p->getResource());
    }
    m_timeline_semaphore_up->increaseTargetValue();
    cmd.addSignalSubmitInfo(m_timeline_semaphore_up->generateSubmitInfo());
    m_write_requests.clear();
}

void VulkanBufferWriter::executeCpuWriteSequence(
    VulkanCommandBufferObject &cmd,
    VulkanBufferProxy &buffer_proxy,
    const BufferWriteSegments &segments) const noexcept
{
    buffer_proxy.writeSegmentsDirectly(segments);
}

void VulkanBufferWriter::executeGpuWriteSequence(
    VulkanCommandBufferObject &cmd,
    VulkanBufferProxy &buffer_proxy,
    const BufferWriteSegments &segments) const noexcept
{
    uint64_t dst_offset = segments.getLowerBoundInBytes();
    uint64_t write_size = segments.getUpperBoundInBytes() - dst_offset;
    VulkanBufferProxy staging_buffer_proxy;
    staging_buffer_proxy.setUsage(GPUBufferUsage::eStaging)
        .create(m_context_p, write_size);
    staging_buffer_proxy.writeSegmentsDirectly(segments, -dst_offset);
    cmd.acquireResource(staging_buffer_proxy.getResource());
    this->copyFromBufferWithBarriers(cmd,
        staging_buffer_proxy.getHandle(), buffer_proxy,
        write_size,
        0, dst_offset);
}

void VulkanBufferWriter::copyFromBufferWithBarriers(
    VulkanCommandBufferObject &cmd,
    vk::Buffer src, VulkanBufferProxy &dst,
    uint64_t data_size_in_bytes,
    uint64_t src_offset_in_bytes, uint64_t dst_offset_in_bytes) const noexcept
{
    auto [src_stage, src_access, dst_stage, dst_access] = vkutils::get_buffer_copy_dependency(dst.getUsage(), cmd.getQueueType());
    if (not m_timeline_semaphore_up->isTargetReached()) {
        vk::BufferMemoryBarrier2 pre_buffer_barrier;
        pre_buffer_barrier.setSrcStageMask(dst_stage)
            .setSrcAccessMask(dst_access)
            .setDstStageMask(src_stage)
            .setDstAccessMask(src_access)
            .setBuffer(dst.getHandle())
            .setOffset(0)
            .setSize(VK_WHOLE_SIZE);
        vk::DependencyInfo pre_dependency;
        pre_dependency.setBufferMemoryBarriers(pre_buffer_barrier);
        cmd.pipelineBarrier2(pre_dependency);
    }
    vk::BufferMemoryBarrier2 post_barrier;
    post_barrier.setSrcStageMask(src_stage)
        .setSrcAccessMask(src_access)
        .setDstStageMask(dst_stage)
        .setDstAccessMask(dst_access)
        .setBuffer(dst.getHandle())
        .setOffset(dst_offset_in_bytes)
        .setSize(data_size_in_bytes);
    vk::DependencyInfo post_dependency;
    post_dependency.setBufferMemoryBarriers(post_barrier);
    vk::BufferCopy copy_region(src_offset_in_bytes, dst_offset_in_bytes, data_size_in_bytes);
    cmd.copyBuffer(src, dst.getHandle(), copy_region);
    cmd.pipelineBarrier2(post_dependency);
}
