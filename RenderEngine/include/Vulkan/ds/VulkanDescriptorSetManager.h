#pragma once

#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/ds/VulkanDescriptorSet2.h"
#include "Vulkan/ds/VulkanBindlessDescriptorSet.h"

namespace lcf::render {

    class VulkanContext;
    
namespace detail {
    class VulkanDescriptorSetAllocator2;
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
        VulkanDescriptorSet2 createSet(const VulkanDescriptorSetLayout2 & layout);
        void destroySet(VulkanDescriptorSet2 && ds);
        std::error_code initBindlessSets(uint32_t frame_copys);
        VulkanBindlessDescriptorSet & getBindlessBufferSet() noexcept { return m_bindless_buffer_set; }
        VulkanBindlessDescriptorSet & getBindlessTextureSet() noexcept { return m_bindless_texture_set; }
    private:
        VulkanContext * m_context_p = nullptr;
        std::unique_ptr<detail::VulkanDescriptorSetAllocator2> m_allocator_up;
        VulkanBindlessDescriptorSet m_bindless_buffer_set;
        VulkanBindlessDescriptorSet m_bindless_texture_set;
    };

}
