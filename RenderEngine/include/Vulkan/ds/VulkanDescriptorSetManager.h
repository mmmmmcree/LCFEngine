#pragma once

#include "Vulkan/ds/details/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/ds/details/VulkanBindlessDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSet2.h"
#include "Vulkan/vulkan_enums.h"
#include <vector>
#include <cstdint>

namespace lcf::render {

    class VulkanContext;
    class VulkanDescriptorSetLayout2;

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

        void create(VulkanContext * context_p);

        // Individual DS — long-lived, individually freeable
        VulkanDescriptorSet2 allocateSet(const VulkanDescriptorSetLayout2 & layout);
        void deallocateSet(VulkanDescriptorSet2 && ds);

        // Bindless initialization (called once per bindless layout)
        void createBindlessSets(const VulkanDescriptorSetLayout2 & buffer_layout,
                                const VulkanDescriptorSetLayout2 & texture_layout);

        // Bindless access — returns the current-frame DS2
        VulkanDescriptorSet2 &       getBindlessBufferSet() noexcept;
        const VulkanDescriptorSet2 & getBindlessBufferSet() const noexcept;
        VulkanDescriptorSet2 &       getBindlessTextureSet() noexcept;
        const VulkanDescriptorSet2 & getBindlessTextureSet() const noexcept;

        // Frame-driven (called by VulkanRenderer)
        void beginFrame(uint32_t frame_index, uint32_t frames_in_flight);

    private:
        VulkanContext * m_context_p = nullptr;
        detail::VulkanDescriptorSetAllocator2 m_allocator;

        detail::VulkanBindlessDescriptorSet m_bindless_buffer_set;   // set=1
        detail::VulkanBindlessDescriptorSet m_bindless_texture_set;  // set=2
    };

}
