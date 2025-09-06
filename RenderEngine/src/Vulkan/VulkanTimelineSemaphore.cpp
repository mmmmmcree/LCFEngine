#include "VulkanTimelineSemaphore.h"
#include "VulkanContext.h"
#include "error.h"
#include <magic_enum/magic_enum.hpp>


bool lcf::render::VulkanTimelineSemaphore::create(VulkanContext * context_p)
{
    if (not context_p or not context_p->isCreated()) { return false; }
    m_context_p = context_p;
    vk::StructureChain<
        vk::SemaphoreCreateInfo,
        vk::SemaphoreTypeCreateInfo> semaphore_info;
    semaphore_info.get<vk::SemaphoreTypeCreateInfo>().setSemaphoreType(vk::SemaphoreType::eTimeline)
        .setInitialValue(m_target_value);
    m_semaphore = m_context_p->getDevice().createSemaphoreUnique(semaphore_info.get());
    return m_semaphore.get();
}

void lcf::render::VulkanTimelineSemaphore::waitFor(uint64_t value) const
{
    auto device = m_context_p->getDevice();
    vk::SemaphoreWaitInfo wait_info;
    wait_info.setSemaphores(m_semaphore.get())
        .setPValues(&value);
    auto result = device.waitSemaphores(wait_info, UINT64_MAX);
    if (result != vk::Result::eSuccess) {
        LCF_THROW_RUNTIME_ERROR(std::format(
            "lcf::render::VulkanTimelineSemaphore::waitFor: Failed to wait for timeline semaphore Error code: {}",
            magic_enum::enum_name(result)));
    }
}

uint64_t lcf::render::VulkanTimelineSemaphore::getCurrentValue() const
{
    auto device = m_context_p->getDevice();
    return device.getSemaphoreCounterValue(m_semaphore.get());
}

vk::SemaphoreSubmitInfo lcf::render::VulkanTimelineSemaphore::generateSubmitInfo() const
{
    return vk::SemaphoreSubmitInfo(m_semaphore.get(), m_target_value);
}
