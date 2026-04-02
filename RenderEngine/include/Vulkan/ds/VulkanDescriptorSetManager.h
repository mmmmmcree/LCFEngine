#pragma once

#include "Vulkan/ds/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/ds/VulkanDescriptorSet2.h"
#include "Vulkan/ds/details/VulkanBindlessDescriptorSet.h"
#include "Vulkan/vulkan_enums.h"
#include <vector>
#include <cstdint>

namespace lcf::render {

    class VulkanContext;
    class VulkanDescriptorSetLayout2;

    class VulkanDescriptorSetManager
    {
    //     using Self = VulkanDescriptorSetManager;
    //     friend class detail::VulkanBindlessDescriptorSet;
    public:
    //     VulkanDescriptorSetManager() = default;
    //     ~VulkanDescriptorSetManager() = default;
    //     VulkanDescriptorSetManager(const Self &) = delete;
    //     Self & operator=(const Self &) = delete;
    //     VulkanDescriptorSetManager(Self &&) noexcept;
    //     Self & operator=(Self &&) noexcept;

        void create(VulkanContext * context_p);

    //     // Per-frame / Persistent DS
    //     VulkanDescriptorSet2 createSet(const VulkanDescriptorSetLayout2 & layout);
    //     void destroySet(VulkanDescriptorSet2 & ds);

    //     // Bindless initialization (called once)
    //     void createBindlessSets(const VulkanDescriptorSetLayout2 & buffer_layout,
    //                             const VulkanDescriptorSetLayout2 & texture_layout);

    //     // Bindless access — returns the current-frame DS2 directly
    //     VulkanDescriptorSet2 &       getBindlessSet(vkenums::DescriptorSetIndex index) noexcept;
    //     const VulkanDescriptorSet2 & getBindlessSet(vkenums::DescriptorSetIndex index) const noexcept;

    //     // Frame-driven (called by VulkanRenderer)
    //     void beginFrame(uint32_t frame_index, uint32_t frames_in_flight);

    // private:
    //     // Internal — called by detail::VulkanBindlessDescriptorSet
    //     VulkanDescriptorSet2 createBindlessSet(const VulkanDescriptorSetLayout2 & layout, uint32_t variable_count);
    //     void destroyBindlessSet(VulkanDescriptorSet2 & ds);
    //     void retireBindlessSet(VulkanDescriptorSet2 && ds, uint32_t retire_after_frame);

    //     void cleanupRetiredSets(uint32_t current_frame);

    //     struct RetiredEntry
    //     {
    //         VulkanDescriptorSet2 set;
    //         uint32_t retire_frame;
    //     };

        VulkanContext * m_context_p = nullptr;
    //     VulkanDescriptorSetAllocator2 m_allocator;
    //     std::vector<RetiredEntry> m_retired_bindless_sets;

    //     detail::VulkanBindlessDescriptorSet m_bindless_buffer_set;   // set=1
    //     detail::VulkanBindlessDescriptorSet m_bindless_texture_set;  // set=2
    };

}
