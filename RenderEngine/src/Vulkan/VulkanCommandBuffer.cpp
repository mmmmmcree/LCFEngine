#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"

bool lcf::render::VulkanCommandBuffer::create(VulkanContext * context_p)
{
    if (not context_p or not context_p->isCreated()) { return false; }
    m_context_p = context_p;
    m_timeline_semaphore_sp = VulkanTimelineSemaphore::makeShared();
    m_timeline_semaphore_sp->create(m_context_p);
    vk::CommandBufferAllocateInfo command_buffer_info;
    command_buffer_info.setCommandPool(m_context_p->getCommandPool())
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);
    auto device = m_context_p->getDevice();
    vk::CommandBuffer::operator=(device.allocateCommandBuffers(command_buffer_info).front());
    return true;
}

void lcf::render::VulkanCommandBuffer::begin(const vk::CommandBufferBeginInfo &begin_info)
{
    m_timeline_semaphore_sp->wait();
    while (not m_resources.empty()) {
        auto resource_sp = m_resources.front();
        if (resource_sp->isPendingComplete()) {
            m_resources.pop();
        }
    }
    /*
        todo
        与command buffer共用一个timeline的resource，
        可以之间按照timeline valu进行清理， <= target value的一次弹出，不用循环判断
    */
    m_wait_infos.clear();
    m_signal_infos.clear();
    this->reset();
    vk::CommandBuffer::begin(begin_info);
    m_context_p->bindCommandBuffer(this);
}

void lcf::render::VulkanCommandBuffer::submit(vk::QueueFlags queue_flags)
{
    m_timeline_semaphore_sp->increaseTargetValue();
    m_signal_infos.emplace_back(m_timeline_semaphore_sp->generateSubmitInfo());
    vk::CommandBufferSubmitInfo command_submit_info;
    command_submit_info.setCommandBuffer(*this);
    vk::SubmitInfo2 submit_info;
    submit_info.setWaitSemaphoreInfos(m_wait_infos)
        .setSignalSemaphoreInfos(m_signal_infos)
        .setCommandBufferInfos(command_submit_info);
    m_context_p->getQueue(queue_flags).submit2(submit_info);
    m_context_p->releaseCommandBuffer();
}

void lcf::render::VulkanCommandBuffer::acquireResource(const VulkanResource::SharedPointer &resource_sp)
{
    m_resources.emplace(resource_sp);
    resource_sp->markAsInUse();
    this->addSignalSubmitInfo(resource_sp->generateSubmitInfo());
}
