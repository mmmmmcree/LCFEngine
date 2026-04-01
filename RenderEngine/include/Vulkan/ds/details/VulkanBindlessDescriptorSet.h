#pragma once

#include "Vulkan/ds/VulkanDescriptorSet2.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/vulkan_constants.h"
#include <vector>
#include <cstdint>

namespace lcf::render {

    class VulkanDescriptorSetManager;

namespace detail {

    class VulkanBindlessDescriptorSet
    {
        using Self = VulkanBindlessDescriptorSet;
    public:
        VulkanBindlessDescriptorSet() = default;
        ~VulkanBindlessDescriptorSet();
        VulkanBindlessDescriptorSet(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanBindlessDescriptorSet(Self && other) noexcept;
        Self & operator=(Self && other) noexcept;

        void create(VulkanDescriptorSetManager * manager_p,
                     const VulkanDescriptorSetLayout2 & layout,
                     uint32_t initial_variable_count,
                     uint32_t frame_copies);
        void destroy();

        void beginFrame(uint32_t frame_index, uint32_t frames_in_flight);

        VulkanDescriptorSet2 &       currentSet() noexcept;
        const VulkanDescriptorSet2 & currentSet() const noexcept;

        void requestGrowth();

    private:
        struct FrameSlot
        {
            VulkanDescriptorSet2 set;
            uint32_t variable_count = 0u;
        };

        enum class GrowthState : uint8_t { eIdle, eBuilding, eSwapPending };

        VulkanDescriptorSetManager * m_manager_p = nullptr;
        const VulkanDescriptorSetLayout2 * m_layout_p = nullptr;
        std::vector<FrameSlot> m_frame_slots;
        uint32_t m_current_frame = 0u;
        GrowthState m_growth_state = GrowthState::eIdle;
        FrameSlot m_incoming_slot;
    };

} // namespace detail

}
