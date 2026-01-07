#include "Vulkan/VulkanTimelineSemaphore.h"
#include "Vulkan/VulkanContext.h"
#include "log.h"
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
    auto result = device.waitSemaphores(wait_info, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess) {
        std::runtime_error error(std::format("Failed to wait for timeline semaphore Error code: {}", magic_enum::enum_name(result)));
        lcf_log_error(error.what());
        throw error;
    }
}

uint64_t lcf::render::VulkanTimelineSemaphore::getCurrentValue() const
{
    return m_context_p->getDevice().getSemaphoreCounterValue(m_semaphore.get());
}

vk::SemaphoreSubmitInfo lcf::render::VulkanTimelineSemaphore::generateSubmitInfo() const noexcept
{
    return vk::SemaphoreSubmitInfo(m_semaphore.get(), m_target_value);
}
