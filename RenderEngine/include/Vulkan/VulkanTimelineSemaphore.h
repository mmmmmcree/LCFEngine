#pragma once

#include "vulkan_fwd_decls.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanTimelineSemaphore : public VulkanTimelineSemaphorePointerDefs
    {
    public:
        VulkanTimelineSemaphore() = default;
        ~VulkanTimelineSemaphore() noexcept = default;
        VulkanTimelineSemaphore(const VulkanTimelineSemaphore &) = delete;
        VulkanTimelineSemaphore & operator=(const VulkanTimelineSemaphore &) = delete;
        VulkanTimelineSemaphore(VulkanTimelineSemaphore && other) noexcept;
        VulkanTimelineSemaphore & operator=(VulkanTimelineSemaphore && other) noexcept;
        bool create(VulkanContext * context_p);
        bool isCreated() const { return m_context_p and m_semaphore; }
        void wait() const { this->waitFor(m_target_value); }
        void waitFor(uint64_t value) const;
        const vk::Semaphore & getHandle() const noexcept { return m_semaphore.get(); }
        void increaseTargetValue() noexcept { ++m_target_value; }
        const uint64_t & getTargetValue() const noexcept { return m_target_value; }
        uint64_t getCurrentValue() const;
        bool isTargetReached() const noexcept { return m_target_value <= this->getCurrentValue(); }
        vk::SemaphoreSubmitInfo generateSubmitInfo() const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        vk::UniqueSemaphore m_semaphore;
        uint64_t m_target_value = 0;
    };
}