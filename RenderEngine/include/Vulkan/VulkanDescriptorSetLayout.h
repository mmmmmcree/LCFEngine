#pragma once

#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"
#include <span>

namespace lcf::render {
    class VulkanContext;

    class VulkanDescriptorSet;

    class VulkanDescriptorSetAllocator;

    class VulkanDescriptorSetLayout : public STDPointerDefs<VulkanDescriptorSetLayout>
    {
        friend class VulkanDescriptorSet;
        friend class VulkanDescriptorSetAllocator;
        using Self = VulkanDescriptorSetLayout;
    public:
        using BindingList = std::vector<vk::DescriptorSetLayoutBinding>;
        using BindingReadSpan = std::span<const vk::DescriptorSetLayoutBinding>;
        using BindingFlagsList = std::vector<vk::DescriptorBindingFlags>;
    public:
        VulkanDescriptorSetLayout() = default;
        ~VulkanDescriptorSetLayout() = default;
        VulkanDescriptorSetLayout(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetLayout(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        Self & setBindings(BindingReadSpan bindings) noexcept { m_binding_list.assign(bindings.begin(), bindings.end()); return *this; }
        Self & setIndex(uint32_t index) noexcept { m_layout_index = index; return *this; }
        uint32_t getIndex() const noexcept { return m_layout_index; }
        void create(VulkanContext * context_p);
        const vk::DescriptorSetLayout & getHandle() const noexcept { return m_layout.get(); }
        BindingReadSpan getBindings() const noexcept { return m_binding_list; }
        bool containsFlags(vk::DescriptorSetLayoutCreateFlags flags) const noexcept { return static_cast<bool>(m_flags & flags); }
    private:
        VulkanContext * m_context_p = nullptr;
        BindingList m_binding_list;
        BindingFlagsList m_binding_flags_list;
        vk::DescriptorSetLayoutCreateFlags m_flags;
        uint32_t m_layout_index = 0;
        vk::UniqueDescriptorSetLayout m_layout;
    };
}