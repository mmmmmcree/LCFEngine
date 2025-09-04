#pragma once

#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;
    class VulkanTimelineSemaphore : public PointerDefs<VulkanTimelineSemaphore>
    {
    public:
        VulkanTimelineSemaphore(VulkanContext * context);
        bool create();
        void wait() const { this->waitFor(m_target_value); }
        void waitFor(uint64_t value) const;
        vk::Semaphore getHandle() const { return m_semaphore.get(); }
        void increaseTargetValue() { ++m_target_value; }
        const uint64_t & getTargetValue() const { return m_target_value; }
        uint64_t getCurrentValue() const;
        bool isTargetReached() const { return m_target_value <= this->getCurrentValue(); }
        vk::SemaphoreSubmitInfo generateSubmitInfo() const;
    private:
        VulkanContext * m_context = nullptr;
        vk::UniqueSemaphore m_semaphore;
        uint64_t m_target_value = 0;
    };
}