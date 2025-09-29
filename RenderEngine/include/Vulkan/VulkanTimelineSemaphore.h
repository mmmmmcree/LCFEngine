#pragma once

#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;
    class VulkanTimelineSemaphore : public STDPointerDefs<VulkanTimelineSemaphore>
    {
    public:
        VulkanTimelineSemaphore() = default;
        VulkanTimelineSemaphore(const VulkanTimelineSemaphore &) = delete;
        VulkanTimelineSemaphore & operator=(const VulkanTimelineSemaphore &) = delete;
        bool create(VulkanContext * context_p);
        bool isCreated() const { return m_context_p and m_semaphore; }
        void wait() const { this->waitFor(m_target_value); }
        void waitFor(uint64_t value) const;
        vk::Semaphore getHandle() const { return m_semaphore.get(); }
        void increaseTargetValue() { ++m_target_value; }
        const uint64_t & getTargetValue() const { return m_target_value; }
        uint64_t getCurrentValue() const;
        bool isTargetReached() const { return m_target_value <= this->getCurrentValue(); }
        vk::SemaphoreSubmitInfo generateSubmitInfo() const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        vk::UniqueSemaphore m_semaphore;
        uint64_t m_target_value = 0;
    };
}