#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_enums.h"
#include "VulkanDescriptorSetBinding.h"
#include <vector>
#include <cstdint>

namespace lcf::render {

    class VulkanContext;
    class VulkanDescriptorSet2;
    class VulkanDescriptorSetLayout2;

namespace detail {

    // ====================================================================
    //  PerFrame: ring-buffer pools, batch vkResetDescriptorPool per frame
    // ====================================================================
    class PerFrameDescriptorSetAllocator
    {
    public:
        PerFrameDescriptorSetAllocator() = default;
        ~PerFrameDescriptorSetAllocator();

        void create(VulkanContext * context_p);
        void destroy();

        VulkanDescriptorSet2 allocate(const VulkanDescriptorSetLayout2 & layout);
        void resetForFrame();

    private:
        vk::DescriptorPool acquirePool();
        vk::DescriptorPool createPool();

        VulkanContext * m_context_p = nullptr;
        std::vector<vk::DescriptorPool> m_available_pools;
        std::vector<vk::DescriptorPool> m_full_pools;
    };

    // ====================================================================
    //  Bindless: eUpdateAfterBind pool, maxSets=1, growth state machine
    // ====================================================================
    class BindlessDescriptorSetAllocator
    {
    public:
        enum class GrowthState : uint8_t { eIdle, eBuilding, eSwapPending };

        BindlessDescriptorSetAllocator() = default;
        ~BindlessDescriptorSetAllocator();

        void create(VulkanContext * context_p);
        void destroy();

        VulkanDescriptorSet2 allocate(const VulkanDescriptorSetLayout2 & layout,
                                      uint32_t initial_variable_count);
        void beginFrame(uint32_t frame_index, uint32_t frames_in_flight);
        void triggerGrowth(const VulkanDescriptorSetLayout2 & layout);

        GrowthState getGrowthState() const noexcept { return m_growth_state; }
        uint32_t    currentCapacity() const noexcept { return m_current_capacity; }

    private:
        vk::DescriptorPool createPool(uint32_t variable_count,
                                       const VulkanDescriptorSetLayout2 & layout);
        vk::DescriptorSet  allocateFromPool(vk::DescriptorPool pool,
                                             const VulkanDescriptorSetLayout2 & layout,
                                             uint32_t variable_count);

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

    // ====================================================================
    //  Persistent: eFreeDescriptorSet pool, individual free
    // ====================================================================
    class PersistentDescriptorSetAllocator
    {
    public:
        PersistentDescriptorSetAllocator() = default;
        ~PersistentDescriptorSetAllocator();

        void create(VulkanContext * context_p);
        void destroy();

        VulkanDescriptorSet2 allocate(const VulkanDescriptorSetLayout2 & layout);
        void free(vk::DescriptorSet descriptor_set);

    private:
        vk::DescriptorPool acquirePool();
        vk::DescriptorPool createPool();

        VulkanContext * m_context_p = nullptr;
        std::vector<vk::DescriptorPool> m_available_pools;
        std::vector<vk::DescriptorPool> m_full_pools;
    };

} // namespace detail

    // ====================================================================
    //  Public dispatcher — owns the three sub-allocators
    // ====================================================================
    class VulkanDescriptorSetAllocator2
    {
        using Self = VulkanDescriptorSetAllocator2;
    public:
        VulkanDescriptorSetAllocator2() = default;
        ~VulkanDescriptorSetAllocator2();
        VulkanDescriptorSetAllocator2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetAllocator2(Self &&) = default;
        Self & operator=(Self &&) = default;

        void create(VulkanContext * context_p);

        VulkanDescriptorSet2 allocate(const VulkanDescriptorSetLayout2 & layout);

        /// Deallocate a descriptor set according to its strategy.
        /// - ePerFrame: no-op (batch reset handles it)
        /// - ePersistent: individual vkFreeDescriptorSets
        /// - eBindless: no-op (pool owns the single set)
        void deallocate(VulkanDescriptorSet2 & ds) const;

        void resetPerFrame();
        void beginFrame(uint32_t frame_index, uint32_t frames_in_flight);

    private:
        VulkanContext * m_context_p = nullptr;

        mutable detail::PerFrameDescriptorSetAllocator   m_per_frame;
        mutable detail::BindlessDescriptorSetAllocator   m_bindless;
        mutable detail::PersistentDescriptorSetAllocator m_persistent;
    };

}
