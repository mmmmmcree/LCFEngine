#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>
#include <system_error>

namespace lcf::vkc {

class TimelineSemaphore
{
    using Self = TimelineSemaphore;
public:
    TimelineSemaphore() = default;
    ~TimelineSemaphore() noexcept = default;
    TimelineSemaphore(const Self &) = delete;
    TimelineSemaphore & operator=(const Self &) = delete;
    TimelineSemaphore(Self && other) noexcept;
    TimelineSemaphore & operator=(Self && other) noexcept;
    std::error_code create(vk::Device device);
    bool isCreated() const { return bool(m_semaphore); }
    std::error_code wait() const noexcept { return this->waitFor(m_target_timestamp); }
    std::error_code waitFor(uint64_t timestamp) const noexcept;
    const vk::Semaphore & getHandle() const noexcept { return m_semaphore.get(); }
    void advanceTarget() noexcept { ++m_target_timestamp; }
    const uint64_t & getTargetTimestamp() const noexcept { return m_target_timestamp; }
    std::expected<uint64_t, std::error_code> getCurrentTimestamp() const noexcept;
    std::expected<bool, std::error_code> isTargetReached(uint64_t timestamp) const noexcept;
    std::expected<bool, std::error_code> isTargetReached() const noexcept { return this->isTargetReached(m_target_timestamp); }
    vk::SemaphoreSubmitInfo generateSubmitInfo() const noexcept;
private:
    vk::Device m_device;
    vk::UniqueSemaphore m_semaphore;
    uint64_t m_target_timestamp = 0;
};

} // namespace lcf::vkc