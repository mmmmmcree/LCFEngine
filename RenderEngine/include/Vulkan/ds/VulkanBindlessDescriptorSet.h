#pragma once

#include "Vulkan/ds/VulkanDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout.h"
#include "Vulkan/vulkan_constants.h"
#include "resource_utils.h"
#include <vulkan/vulkan.hpp>
#include <memory>
#include <vector>
#include <deque>
#include <tsl/robin_map.h>

namespace lcf::render {

namespace detail {
    class VulkanBindlessDescriptorSetAllocator;
}

    class VulkanBindlessDescriptorSet
    {
        using Self = VulkanBindlessDescriptorSet;
        using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
        struct AuthorityBinding
        {
            DescriptorInfo descriptor_info;
            ResourceLease lease;
        };
        using AuthorityBindingMap = std::vector<tsl::robin_map<uint32_t, AuthorityBinding>>;
        struct Slot
        {
            using LeaseMap = std::unordered_map<uint64_t, ResourceLease>;
            Slot() = default;
            Slot(VulkanDescriptorSet set, uint32_t variable_count);
            void addDescriptorInfo(uint32_t binding, uint32_t array_index, const DescriptorInfo & info);
            void addLease(uint32_t binding, uint32_t array_index, ResourceLease lease);
            void commitUpdate(vk::Device device);
            VulkanDescriptorSet m_set;
            uint32_t m_variable_count = vkconstants::ds::k_initial_variable_descriptor_count >> 1;
            LeaseMap m_in_use_resource_leases;
            LeaseMap m_pending_resource_leases;
        };
        using FrameSlots = std::vector<Slot>;
        using RetiredSlots = std::deque<Slot>;
    public:
        VulkanBindlessDescriptorSet() = default;
        ~VulkanBindlessDescriptorSet() noexcept;
        VulkanBindlessDescriptorSet(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanBindlessDescriptorSet(Self && other) noexcept = default;
        Self & operator=(Self && other) noexcept = default;
        operator bool() const noexcept;
    public:
        std::error_code create(
            vk::Device device,
            vkenums::BindlessSetType bindless_set_type,
            uint32_t frame_copies) noexcept;
        Self & addDescriptorInfo(uint32_t binding, uint32_t array_index, const DescriptorInfo & info);
        Self & addDescriptorInfo(uint32_t binding, uint32_t array_index, const DescriptorInfo & info, ResourceLease lease);
        Self & addDescriptorInfo(uint32_t binding, const DescriptorInfo & info) { return this->addDescriptorInfo(binding, 0u, info); }
        void commitUpdate(vk::Device device) noexcept;
        const vk::DescriptorSet & getHandle() const noexcept;
        uint32_t getIndex() const noexcept { return m_layout.getIndex(); }
        vkenums::DescriptorSetStrategy getStrategy() const noexcept { return m_layout.getStrategy(); }
        std::span<const VulkanDescriptorSetBinding> getBindings() const noexcept { return m_layout.getBindings(); }
    private:
        std::error_code recreateSlot(vk::Device device, Slot & slot);
    private:
        std::unique_ptr<detail::VulkanBindlessDescriptorSetAllocator> m_allocator_up;
        VulkanDescriptorSetLayout m_layout;
        AuthorityBindingMap m_authority_binding_map;
        FrameSlots m_frame_slots;
        RetiredSlots m_retired_slots;
        uint32_t m_current_index = 0u;
    };
} // namespace lcf::render
