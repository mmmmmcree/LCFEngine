#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_enums.h"
#include "Vulkan/ds/VulkanDescriptorSetBinding.h"
#include <span>
#include <vector>

namespace lcf::render {
    class VulkanDescriptorSetLayout
    {
        using Self = VulkanDescriptorSetLayout;
    public:
        using BindingList = std::vector<VulkanDescriptorSetBinding>;
        using BindingReadSpan  = std::span<const VulkanDescriptorSetBinding>;
    public:
        VulkanDescriptorSetLayout() = default;
        ~VulkanDescriptorSetLayout() = default;
        VulkanDescriptorSetLayout(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetLayout(Self &&) = default;
        Self & operator=(Self &&) = default;
        operator vk::DescriptorSetLayout() const noexcept { return this->getHandle(); }
    public:
        std::error_code create(vk::Device device, vkenums::DescriptorSetStrategy strategy) noexcept;
        Self & setBindings(BindingReadSpan bindings) noexcept;
        Self & setIndex(uint32_t index) noexcept;
        uint32_t getIndex() const noexcept { return m_layout_index; }
        vkenums::DescriptorSetStrategy getStrategy() const noexcept { return m_strategy; }
        const VulkanDescriptorSetLayoutBindings & getBindings() const noexcept { return m_bindings; }
        const vk::DescriptorSetLayout & getHandle() const noexcept { return m_layout.get(); }
    private:
        VulkanDescriptorSetLayoutBindings m_bindings;
        vkenums::DescriptorSetStrategy m_strategy = vkenums::DescriptorSetStrategy::eIndividual;
        uint32_t m_layout_index = 0u;
        vk::UniqueDescriptorSetLayout m_layout;
    };

}
