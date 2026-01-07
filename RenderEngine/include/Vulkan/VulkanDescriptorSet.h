#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanDescriptorSetLayout.h"
#include "VulkanDescriptorSetUpdater.h"
#include "PointerDefs.h"
#include <optional>
#include <span>

namespace lcf::render {
    class VulkanContext;

    class VulkanDescriptorSetAllocator;

    class VulkanDescriptorSet : public STDPointerDefs<VulkanDescriptorSet>
    {
        friend class VulkanDescriptorSetAllocator;
        using Self = VulkanDescriptorSet;
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<VulkanDescriptorSet>);
        using BindingReadSpan = std::span<const vk::DescriptorSetLayoutBinding>;
        VulkanDescriptorSet() = default;
        ~VulkanDescriptorSet();
        VulkanDescriptorSet(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSet(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        bool create(const VulkanDescriptorSetLayout::SharedPointer & layout_sp);
        Self & setIndex(uint32_t index) noexcept { m_set_index = index; return *this; }
        uint32_t getIndex() const noexcept { return m_set_index; }
        const vk::DescriptorSet & getHandle() const noexcept { return m_descriptor_set; }
        const VulkanDescriptorSetLayout & getLayout() const noexcept { return *m_layout_sp; }
        VulkanDescriptorSetUpdater generateUpdater() const noexcept;
    private:
        VulkanDescriptorSetLayout::SharedPointer m_layout_sp;
        std::optional<vk::DescriptorPool> m_descriptor_pool_opt;
        uint32_t m_set_index = 0u;
        vk::DescriptorSet m_descriptor_set;
    };

}