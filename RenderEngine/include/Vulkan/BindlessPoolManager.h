#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanDescriptorSet2.h"
#include <vector>
#include <cstdint>

namespace lcf::render {

    class VulkanContext;
    class VulkanDescriptorSetLayout2;

    class BindlessPoolManager
    {
        using Self = BindlessPoolManager;
    public:
        enum class GrowthState : uint8_t { eIdle, eBuilding, eSwapPending };

        BindlessPoolManager() = default;
        ~BindlessPoolManager();
        BindlessPoolManager(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        BindlessPoolManager(Self &&) = default;
        Self & operator=(Self &&) = default;

        void create(VulkanContext * context_p);
        VulkanDescriptorSet2 allocate(const VulkanDescriptorSetLayout2 & layout,
                                      uint32_t initial_variable_count);
        void beginFrame(uint32_t frame_index, uint32_t frames_in_flight);

        GrowthState getGrowthState() const noexcept { return m_growth_state; }
        uint32_t currentCapacity() const noexcept { return m_current_capacity; }

    private:
        vk::DescriptorPool createPool(uint32_t variable_count,
                                      const VulkanDescriptorSetLayout2 & layout);
        vk::DescriptorSet allocateFromPool(vk::DescriptorPool pool,
                                           const VulkanDescriptorSetLayout2 & layout,
                                           uint32_t variable_count);
        void triggerGrowth(const VulkanDescriptorSetLayout2 & layout);

        VulkanContext * m_context_p = nullptr;

        vk::DescriptorPool m_pool;
        vk::DescriptorSet  m_descriptor_set;
        uint32_t           m_current_capacity = 0u;

        GrowthState        m_growth_state = GrowthState::eIdle;
        vk::DescriptorPool m_incoming_pool;
        vk::DescriptorSet  m_incoming_set;
        uint32_t           m_incoming_capacity = 0u;

        struct RetiredPool { vk::DescriptorPool pool; uint32_t retire_frame; };
        std::vector<RetiredPool> m_retired_pools;

        const VulkanDescriptorSetLayout2 * m_layout_p = nullptr;
    };

}
