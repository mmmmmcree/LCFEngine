#include "VulkanCommandBufferObject.h"
#include "VulkanContext.h"

bool lcf::render::VulkanCommandBufferObject::create(VulkanContext * context_p, vk::QueueFlagBits queue_type)
{
    if (not context_p or not context_p->isCreated()) { return false; }
    m_context_p = context_p;
    m_queue_type = queue_type;
    m_timeline_semaphore_sp = VulkanTimelineSemaphore::makeShared();
    m_timeline_semaphore_sp->create(m_context_p);
    vk::CommandBufferAllocateInfo command_buffer_info;
    command_buffer_info.setCommandPool(m_context_p->getCommandPool(queue_type))
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);
    auto device = m_context_p->getDevice();
    vk::CommandBuffer::operator=(device.allocateCommandBuffers(command_buffer_info).front());
    return true;
}

void lcf::render::VulkanCommandBufferObject::prepareForRecording()
{
    m_timeline_semaphore_sp->wait();
    m_resources.clear();
    m_wait_infos.clear();
    m_signal_infos.clear();
    this->reset();
}

void lcf::render::VulkanCommandBufferObject::begin(const vk::CommandBufferBeginInfo &begin_info)
{
    vk::CommandBuffer::begin(begin_info);
}

void lcf::render::VulkanCommandBufferObject::end()
{
    vk::CommandBuffer::end();
}

void lcf::render::VulkanCommandBufferObject::submit()
{
    m_timeline_semaphore_sp->increaseTargetValue();
    m_signal_infos.emplace_back(m_timeline_semaphore_sp->generateSubmitInfo());
    vk::CommandBufferSubmitInfo command_submit_info;
    command_submit_info.setCommandBuffer(*this);
    vk::SubmitInfo2 submit_info;
    submit_info.setWaitSemaphoreInfos(m_wait_infos)
        .setSignalSemaphoreInfos(m_signal_infos)
        .setCommandBufferInfos(command_submit_info);
    m_context_p->getQueue(m_queue_type).submit2(submit_info);
}

void lcf::render::VulkanCommandBufferObject::acquireResource(const GPUResource::SharedPointer &resource_sp)
{
    m_resources.emplace_back(resource_sp);
}
