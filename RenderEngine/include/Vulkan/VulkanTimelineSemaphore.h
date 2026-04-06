#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanTimelineSemaphore
    {
    public:
        VulkanTimelineSemaphore() = default;
        ~VulkanTimelineSemaphore() noexcept = default;
        VulkanTimelineSemaphore(const VulkanTimelineSemaphore &) = delete;
        VulkanTimelineSemaphore & operator=(const VulkanTimelineSemaphore &) = delete;
        VulkanTimelineSemaphore(VulkanTimelineSemaphore && other) noexcept;
        VulkanTimelineSemaphore & operator=(VulkanTimelineSemaphore && other) noexcept;
        std::error_code create(vk::Device device);
        bool isCreated() const { return bool(m_semaphore); }
        std::error_code wait() const noexcept { return this->waitFor(m_target_value); }
        std::error_code waitFor(uint64_t value) const noexcept;
        const vk::Semaphore & getHandle() const noexcept { return m_semaphore.get(); }
        void increaseTargetValue() noexcept { ++m_target_value; }
        const uint64_t & getTargetValue() const noexcept { return m_target_value; }
        uint64_t getCurrentValue() const;
        bool isTargetReached() const noexcept { return m_target_value <= this->getCurrentValue(); }
        vk::SemaphoreSubmitInfo generateSubmitInfo() const noexcept;
    private:
        vk::Device m_device;
        vk::UniqueSemaphore m_semaphore;
        uint64_t m_target_value = 0;
    };
}