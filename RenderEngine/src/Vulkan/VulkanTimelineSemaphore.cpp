#include "VulkanTimelineSemaphore.h"
#include "VulkanContext.h"

lcf::render::VulkanTimelineSemaphore::VulkanTimelineSemaphore(VulkanContext *context) :
    m_context(context)
{
}


bool lcf::render::VulkanTimelineSemaphore::create()
{
    vk::StructureChain<
        vk::SemaphoreCreateInfo,
        vk::SemaphoreTypeCreateInfo> semaphore_info;
    semaphore_info.get<vk::SemaphoreTypeCreateInfo>().setSemaphoreType(vk::SemaphoreType::eTimeline)
        .setInitialValue(m_target_value);
    m_semaphore = m_context->getDevice().createSemaphoreUnique(semaphore_info.get());
    return m_semaphore.get();
}

void lcf::render::VulkanTimelineSemaphore::waitFor(uint64_t value) const
{
    auto device = m_context->getDevice();
    vk::SemaphoreWaitInfo wait_info;
    wait_info.setSemaphores(m_semaphore.get())
        .setPValues(&value);
    auto result = device.waitSemaphores(wait_info, UINT64_MAX);
    if (result != vk::Result::eSuccess) {
        const char *error_string = "Failed to wait for timeline semaphore";
        qDebug() << error_string;
        throw std::runtime_error(error_string);
    }
}

uint64_t lcf::render::VulkanTimelineSemaphore::getCurrentValue() const
{
    auto device = m_context->getDevice();
    return device.getSemaphoreCounterValue(m_semaphore.get());
}

vk::SemaphoreSubmitInfo lcf::render::VulkanTimelineSemaphore::generateSubmitInfo() const
{
    return vk::SemaphoreSubmitInfo(m_semaphore.get(), m_target_value);
}
