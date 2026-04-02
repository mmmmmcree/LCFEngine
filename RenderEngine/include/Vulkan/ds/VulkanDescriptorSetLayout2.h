#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_enums.h"
#include "Vulkan/ds/VulkanDescriptorSetBinding.h"
#include <span>
#include <vector>

namespace lcf::render {
    class VulkanDescriptorSetLayout2
    {
        using Self = VulkanDescriptorSetLayout2;
    public:
        using BindingList      = std::vector<VulkanDescriptorSetBinding>;
        using BindingFlagsList = std::vector<vk::DescriptorBindingFlags>;
        using BindingReadSpan  = std::span<const VulkanDescriptorSetBinding>;
    public:
        VulkanDescriptorSetLayout2() = default;
        ~VulkanDescriptorSetLayout2() = default;
        VulkanDescriptorSetLayout2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetLayout2(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        std::error_code create(vk::Device device) noexcept;
        Self & setBindings(BindingReadSpan bindings) noexcept;
        Self & setIndex(uint32_t index) noexcept;
        uint32_t getIndex() const noexcept { return m_layout_index; }
        vkenums::DescriptorSetStrategy getStrategy() const noexcept { return m_strategy; }
        BindingReadSpan getBindings() const noexcept { return m_binding_list; }
        const vk::DescriptorSetLayout & getHandle() const noexcept { return m_layout.get(); }
    private:
        BindingList m_binding_list;
        vkenums::DescriptorSetStrategy m_strategy = vkenums::DescriptorSetStrategy::eIndividual;
        uint32_t m_layout_index = 0u;
        vk::UniqueDescriptorSetLayout m_layout;
    };

}
