#pragma once

#include <vulkan/vulkan.hpp>

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
    std::error_code wait() const noexcept { return this->waitFor(m_target_value); }
    std::error_code waitFor(uint64_t value) const noexcept;
    const vk::Semaphore & getHandle() const noexcept { return m_semaphore.get(); }
    void increaseTargetValue() noexcept { ++m_target_value; }
    const uint64_t & getTargetValue() const noexcept { return m_target_value; }
    uint64_t getCurrentValue() const;
    bool isTargetReached(uint64_t value) const noexcept { return value <= this->getCurrentValue(); }
    bool isTargetReached() const noexcept { return this->isTargetReached(m_target_value); }
    vk::SemaphoreSubmitInfo generateSubmitInfo() const noexcept;
private:
    vk::Device m_device;
    vk::UniqueSemaphore m_semaphore;
    uint64_t m_target_value = 0;
};

} // namespace lcf::vkc