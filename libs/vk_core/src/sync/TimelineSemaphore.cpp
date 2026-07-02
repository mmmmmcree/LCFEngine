#include "vk_core/sync/TimelineSemaphore.h"
#include "vk_core/sync/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include <limits>

namespace lcf::vkc {

void register_timeline_semaphore(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceVulkan12Features::timelineSemaphore),
    };
    manifest.addRequiredFeatures(k_features);
}

TimelineSemaphore::TimelineSemaphore(Self && other) noexcept :
    m_device(std::exchange(other.m_device, nullptr)),
    m_semaphore(std::move(other.m_semaphore)),
    m_target_timestamp(std::exchange(other.m_target_timestamp, 0))
{
}

auto TimelineSemaphore::operator=(Self && other) noexcept -> Self &
{
    if (this == &other) { return *this; }
    m_device = std::exchange(other.m_device, nullptr);
    m_semaphore = std::move(other.m_semaphore);
    m_target_timestamp = std::exchange(other.m_target_timestamp, 0);
    return *this;
}

std::error_code TimelineSemaphore::create(vk::Device device)
{
    m_device = device;
    vk::StructureChain<
        vk::SemaphoreCreateInfo,
        vk::SemaphoreTypeCreateInfo> semaphore_info;
    semaphore_info.get<vk::SemaphoreTypeCreateInfo>().setSemaphoreType(vk::SemaphoreType::eTimeline)
        .setInitialValue(m_target_timestamp);
    try {
        m_semaphore = device.createSemaphoreUnique(semaphore_info.get());
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

std::error_code TimelineSemaphore::waitFor(uint64_t timestamp) const noexcept
{
    vk::SemaphoreWaitInfo wait_info;
    wait_info.setSemaphores(m_semaphore.get())
        .setPValues(&timestamp);
    try {
        vk::Result result = m_device.waitSemaphores(wait_info, std::numeric_limits<uint64_t>::max());
        if (result == vk::Result::eTimeout) { return std::make_error_code(std::errc::timed_out); }
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

std::expected<uint64_t, std::error_code> TimelineSemaphore::getCurrentTimestamp() const noexcept
{
    uint64_t timestamp;
    try {
        timestamp = m_device.getSemaphoreCounterValue(m_semaphore.get());
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    return timestamp;
}

std::expected<bool, std::error_code> lcf::vkc::TimelineSemaphore::isTargetReached(uint64_t timestamp) const noexcept
{
    return this->getCurrentTimestamp().transform([timestamp](uint64_t current) { return timestamp <= current; });
}

vk::SemaphoreSubmitInfo TimelineSemaphore::generateSubmitInfo() const noexcept
{
    return vk::SemaphoreSubmitInfo(m_semaphore.get(), m_target_timestamp);
}

} // namespace lcf::vkc