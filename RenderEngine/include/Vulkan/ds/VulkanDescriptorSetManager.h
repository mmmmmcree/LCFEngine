#pragma once

#include "Vulkan/ds/details/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/ds/details/VulkanBindlessDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSet2.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/vulkan_enums.h"
#include <vector>
#include <cstdint>

namespace lcf::render {

    class VulkanContext;

    class VulkanDescriptorSetManager
    {
        using Self = VulkanDescriptorSetManager;
    public:
        VulkanDescriptorSetManager() = default;
        ~VulkanDescriptorSetManager() = default;
        VulkanDescriptorSetManager(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetManager(Self &&) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
    public:
        // Creates allocator, builds bindless layouts internally, and initialises bindless sets.
        void create(VulkanContext * context_p);
        // Individual DS — long-lived, individually freeable
        VulkanDescriptorSet2 allocateSet(const VulkanDescriptorSetLayout2 & layout);
        void deallocateSet(VulkanDescriptorSet2 && ds);
        // Bindless writes — changes are staged and flushed lazily on beginFrame.
        void addBindlessBufferInfo(uint32_t binding, uint32_t array_index, const vk::DescriptorBufferInfo & info);
        void addBindlessTextureInfo(uint32_t binding, uint32_t array_index, const vk::DescriptorImageInfo & info);
        // Bindless access — returns the current-frame DS (valid after beginFrame)
        VulkanDescriptorSet2 & getBindlessBufferSet() noexcept;
        const VulkanDescriptorSet2 & getBindlessBufferSet() const noexcept;
        VulkanDescriptorSet2 & getBindlessTextureSet() noexcept;
        const VulkanDescriptorSet2 & getBindlessTextureSet() const noexcept;
        void beginFrame(uint32_t frame_index, uint32_t frames_in_flight);
    private:
        void createBindlessLayouts(vk::Device device);
        void createBindlessSets();
    private:
        VulkanContext * m_context_p = nullptr;
        detail::VulkanDescriptorSetAllocator2 m_allocator;
        // Layouts are owned here so their lifetime exceeds that of the bindless sets.
        VulkanDescriptorSetLayout2 m_bindless_buffer_layout;
        VulkanDescriptorSetLayout2 m_bindless_texture_layout;
        detail::VulkanBindlessDescriptorSet m_bindless_buffer_set;
        detail::VulkanBindlessDescriptorSet m_bindless_texture_set;
    };

}
