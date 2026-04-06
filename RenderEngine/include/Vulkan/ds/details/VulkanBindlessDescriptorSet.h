#pragma once

#include "Vulkan/ds/VulkanDescriptorSet2.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <deque>
#include <tsl/robin_map.h>

namespace lcf::render {

    class VulkanDescriptorSetLayout2;

namespace detail {

    class VulkanDescriptorSetAllocator2;

    // Manages per-frame copies of a bindless DescriptorSet with automatic growth.
    //
    // Per-frame copies ensure CPU writes to one frame's DS never race with GPU
    // reads from another in-flight frame.
    // Within a single frame, eUpdateAfterBind | ePartiallyBound | eUpdateUnusedWhilePending
    // allow safe incremental updates without pipeline stalls.
    //
    // Growth (variable descriptor count exceeded):
    //   All frame slots are reallocated with doubled capacity and fully repopulated
    //   from the authority map. Old slots are retired and reclaimed once no longer in-flight.
    //
    // Usage:
    //   addDescriptorInfo(binding, array_index, info)  — stage write to all frame copies
    //   beginFrame(frame_index, frames_in_flight)      — commit pending writes for current frame / grow if needed
    //   getSet()                                       — current frame's DS handle
    class VulkanBindlessDescriptorSet
    {
        using Self = VulkanBindlessDescriptorSet;
        using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
        using AuthorityBindingMap = std::vector<tsl::robin_map<uint32_t, DescriptorInfo>>;
        struct Slot
        {
            VulkanDescriptorSet2 set;
            uint32_t variable_count = 0u;
        };
        using FrameSlots = std::vector<Slot>;
        using RetiredSlots = std::deque<Slot>;
    public:
        VulkanBindlessDescriptorSet() = default;
        ~VulkanBindlessDescriptorSet();
        VulkanBindlessDescriptorSet(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanBindlessDescriptorSet(Self && other) noexcept;
        Self & operator=(Self && other) noexcept;
    public:
        void create(
            VulkanDescriptorSetAllocator2 & allocator,
            const VulkanDescriptorSetLayout2 & layout,
            uint32_t binding_count,
            uint32_t initial_variable_count,
            uint32_t frame_copies) noexcept;
        void destroy();
        // Stage a write to all frame copies' pending lists.
        Self & addDescriptorInfo(uint32_t binding, uint32_t array_index, const DescriptorInfo & info);
        // Advance to frame_index: commit pending writes for the current frame slot,
        // growing all slots if the variable binding capacity is exceeded.
        void beginFrame(uint32_t frame_index, uint32_t frames_in_flight, vk::Device device);
        VulkanDescriptorSet2 & getSet() noexcept;
        const VulkanDescriptorSet2 & getSet() const noexcept;
    private:
        uint32_t computeRequiredCount() const noexcept;
        Slot allocateSlot(uint32_t variable_count);
        void writeAll(VulkanDescriptorSet2 & set, vk::Device device);
    private:
        VulkanDescriptorSetAllocator2 * m_allocator_p = nullptr;
        const VulkanDescriptorSetLayout2 * m_layout_p = nullptr;
        FrameSlots m_frame_slots;
        RetiredSlots m_retired_slots;
        uint32_t m_current_frame = 0u;
        AuthorityBindingMap m_authority_binding_map;
    };

} // namespace detail
} // namespace lcf::render
