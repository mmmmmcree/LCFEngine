#include "Vulkan/VulkanBufferController.h"

using namespace lcf::render;

void VulkanBufferController::create(VulkanContext *context_p)
{
    m_context_p = context_p;
    m_timeline_semaphore_up = VulkanTimelineSemaphore::makeUnique();
    m_timeline_semaphore_up->create(m_context_p);
} 

void VulkanBufferController::executeWriteSequence(
    VulkanCommandBufferObject &cmd,
    std::span<VulkanBufferProxy> buffer_proxies,
    std::span<const BufferWriteSegments> segments_span)
{
    size_t count = segments_span.size();
    if (count != buffer_proxies.size()) { return; }
    using ExecuteWriteSequenceMethod = void (Self::*)(VulkanCommandBufferObject &, VulkanBufferProxy &, const BufferWriteSegments &);
    ExecuteWriteSequenceMethod execute_write_sequence_method = &Self::executeGpuWriteSequence;
    if (m_pattern == GPUBufferPattern::eDynamic and m_timeline_semaphore_up->isTargetReached()) {
        execute_write_sequence_method = &Self::executeCpuWriteSequence;
    }
    auto timeline_value = m_timeline_semaphore_up->getCurrentValue();
    for (size_t i = 0; i < count; ++i) {
        (this->*execute_write_sequence_method)(cmd, buffer_proxies[i], segments_span[i]);
    }
    if (m_timeline_semaphore_up->getCurrentValue() != timeline_value) {
        cmd.addSignalSubmitInfo(m_timeline_semaphore_up->generateSubmitInfo());
    }
}