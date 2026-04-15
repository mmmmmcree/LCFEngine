#pragma once

#include "Vulkan/ds/VulkanDescriptorSetLayout.h"
#include "Vulkan/ds/VulkanDescriptorSet.h"
#include "Vulkan/ds/VulkanBindlessDescriptorSet.h"

namespace lcf::render {

    class VulkanContext;
    
namespace detail {
    class VulkanDescriptorSetAllocator;
}

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
        std::error_code create(VulkanContext & context) noexcept;
        VulkanDescriptorSet createSet(const VulkanDescriptorSetLayout & layout);
        void destroySet(VulkanDescriptorSet && ds);
        std::error_code initBindlessSets(uint32_t frame_copys);
        VulkanBindlessDescriptorSet & getBindlessBufferSet() noexcept { return m_bindless_buffer_set; }
        VulkanBindlessDescriptorSet & getBindlessTextureSet() noexcept { return m_bindless_texture_set; }
    private:
        VulkanContext * m_context_p = nullptr;
        std::unique_ptr<detail::VulkanDescriptorSetAllocator> m_allocator_up;
        VulkanBindlessDescriptorSet m_bindless_buffer_set;
        VulkanBindlessDescriptorSet m_bindless_texture_set;
    };

}
