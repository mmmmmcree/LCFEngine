#pragma once

#include "PerFramePoolManager.h"
#include "BindlessPoolManager.h"
#include "PersistentPoolManager.h"

namespace lcf::render {

    class VulkanContext;
    class VulkanDescriptorSetLayout2;

    class VulkanDescriptorSetAllocator2
    {
        using Self = VulkanDescriptorSetAllocator2;
    public:
        VulkanDescriptorSetAllocator2() = default;
        ~VulkanDescriptorSetAllocator2() = default;
        VulkanDescriptorSetAllocator2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetAllocator2(Self &&) = default;
        Self & operator=(Self &&) = default;

        void create(VulkanContext * context_p);
        VulkanDescriptorSet2 allocate(const VulkanDescriptorSetLayout2 & layout);
        void resetPerFrame();
        void beginFrame(uint32_t frame_index, uint32_t frames_in_flight);

        PerFramePoolManager   & getPerFrameManager()   noexcept { return m_per_frame_manager; }
        BindlessPoolManager   & getBindlessManager()   noexcept { return m_bindless_manager; }
        PersistentPoolManager & getPersistentManager() noexcept { return m_persistent_manager; }

    private:
        PerFramePoolManager   m_per_frame_manager;
        BindlessPoolManager   m_bindless_manager;
        PersistentPoolManager m_persistent_manager;
    };

}
