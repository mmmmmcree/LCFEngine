#include "vk_core/sync/TimelineSemaphore.h"

using namespace lcf::vkc;

TimelineSemaphore::TimelineSemaphore(Self && other) noexcept :
    m_device(std::exchange(other.m_device, nullptr)),
    m_semaphore(std::move(other.m_semaphore)),
    m_target_value(std::exchange(other.m_target_value, 0))
{
}

auto TimelineSemaphore::operator=(Self && other) noexcept -> Self &
{
    if (this == &other) { return *this; }
    m_device = std::exchange(other.m_device, nullptr);
    m_semaphore = std::move(other.m_semaphore);
    m_target_value = std::exchange(other.m_target_value, 0);
    return *this;
}

std::error_code TimelineSemaphore::create(vk::Device device)
{
    if (not device) { return std::make_error_code(std::errc::invalid_argument); }
    m_device = device;
    vk::StructureChain<
        vk::SemaphoreCreateInfo,
        vk::SemaphoreTypeCreateInfo> semaphore_info;
    semaphore_info.get<vk::SemaphoreTypeCreateInfo>().setSemaphoreType(vk::SemaphoreType::eTimeline)
        .setInitialValue(m_target_value);
    try {
        m_semaphore = device.createSemaphoreUnique(semaphore_info.get());
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

std::error_code TimelineSemaphore::waitFor(uint64_t value) const noexcept
{
    vk::SemaphoreWaitInfo wait_info;
    wait_info.setSemaphores(m_semaphore.get())
        .setPValues(&value);
    vk::Result result = m_device.waitSemaphores(wait_info, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess) {
        return std::make_error_code(std::errc::timed_out);
    }
    return {};
}

uint64_t TimelineSemaphore::getCurrentValue() const
{
    return m_device.getSemaphoreCounterValue(m_semaphore.get());
}

vk::SemaphoreSubmitInfo TimelineSemaphore::generateSubmitInfo() const noexcept
{
    return vk::SemaphoreSubmitInfo(m_semaphore.get(), m_target_value);
}
