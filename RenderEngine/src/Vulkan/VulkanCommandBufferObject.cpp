#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanTimelineSemaphore.h"
#include "Vulkan/VulkanPipeline.h"
#include "Vulkan/ds/VulkanDescriptorSet.h"

using namespace lcf::render;

VulkanCommandBufferObject::~VulkanCommandBufferObject() noexcept = default;

std::error_code VulkanCommandBufferObject::create(VulkanContext *context_p, vk::QueueFlagBits queue_type)
{
    if (not context_p or not context_p->isCreated()) { return std::make_error_code(std::errc::invalid_argument); }
    m_context_p = context_p;
    m_queue_type = queue_type;
    m_timeline_semaphore_sp = std::make_shared<VulkanTimelineSemaphore>();
    if (auto ec = m_timeline_semaphore_sp->create(m_context_p->getDevice())) { return ec; }
    vk::CommandBufferAllocateInfo command_buffer_info;
    command_buffer_info.setCommandPool(m_context_p->getCommandPool(queue_type))
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);
    auto device = m_context_p->getDevice();
    vk::CommandBuffer::operator=(device.allocateCommandBuffers(command_buffer_info).front());
    return {};
}

void VulkanCommandBufferObject::waitUntilAvailable()
{
    m_timeline_semaphore_sp->wait();
}

void VulkanCommandBufferObject::begin(const vk::CommandBufferBeginInfo &begin_info)
{
    m_resource_leases.clear();
    m_wait_infos.clear();
    m_signal_infos.clear();
    this->reset();
    vk::CommandBuffer::begin(begin_info);
}

void VulkanCommandBufferObject::end()
{
    vk::CommandBuffer::end();
}

vk::SemaphoreSubmitInfo VulkanCommandBufferObject::submit()
{
    m_timeline_semaphore_sp->increaseTargetValue();
    auto submission_complete_info = m_timeline_semaphore_sp->generateSubmitInfo();
    m_signal_infos.emplace_back(submission_complete_info);
    vk::CommandBufferSubmitInfo command_submit_info;
    command_submit_info.setCommandBuffer(*this);
    vk::SubmitInfo2 submit_info;
    submit_info.setWaitSemaphoreInfos(m_wait_infos)
        .setSignalSemaphoreInfos(m_signal_infos)
        .setCommandBufferInfos(command_submit_info);
    m_context_p->getQueue(m_queue_type).submit2(submit_info);
    return submission_complete_info;
}

void VulkanCommandBufferObject::acquireResourceLease(ResourceLease resource_lease)
{
    m_resource_leases.emplace_back(std::move(resource_lease));
}

void VulkanCommandBufferObject::bindPipeline(const VulkanPipeline &pipeline) const noexcept
{
    Base::bindPipeline(pipeline.getType(), pipeline.getHandle());
}

void VulkanCommandBufferObject::bindDescriptorSet(const VulkanPipeline &pipeline, const VulkanDescriptorSet &descriptor_set) const noexcept
{
    Base::bindDescriptorSets(pipeline.getType(), pipeline.getPipelineLayout(), descriptor_set.getIndex(), descriptor_set.getHandle(), nullptr);
}

void VulkanCommandBufferObject::bindDescriptorSet(const VulkanPipeline &pipeline, const VulkanBindlessDescriptorSet &descriptor_set) const noexcept
{
    Base::bindDescriptorSets(pipeline.getType(), pipeline.getPipelineLayout(), descriptor_set.getIndex(), descriptor_set.getHandle(), nullptr);
}