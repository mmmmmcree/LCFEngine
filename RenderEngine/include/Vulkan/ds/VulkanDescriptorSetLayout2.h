#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_enums.h"
#include "Vulkan/ds/VulkanDescriptorSetBinding.h"
#include <span>
#include <vector>

namespace lcf::render {

    class VulkanContext;

    class VulkanDescriptorSetLayout2
    {
        using Self = VulkanDescriptorSetLayout2;
    public:
        using BindingList      = std::vector<VulkanDescriptorSetBinding>;
        using BindingFlagsList = std::vector<vk::DescriptorBindingFlags>;
        using BindingReadSpan  = std::span<const VulkanDescriptorSetBinding>;

        VulkanDescriptorSetLayout2() = default;
        ~VulkanDescriptorSetLayout2() = default;
        VulkanDescriptorSetLayout2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetLayout2(Self &&) = default;
        Self & operator=(Self &&) = default;

        Self & setBindings(BindingReadSpan bindings);
        Self & setIndex(uint32_t index) noexcept;
        Self & setStrategy(vkenums::DescriptorSetStrategy strategy) noexcept;

        uint32_t getIndex() const noexcept { return m_layout_index; }
        vkenums::DescriptorSetStrategy getStrategy() const noexcept { return m_strategy; }
        BindingReadSpan getBindings() const noexcept { return m_binding_list; }
        const vk::DescriptorSetLayout & getHandle() const noexcept { return m_layout.get(); }

        void create(VulkanContext * context_p);

    private:
        VulkanContext * m_context_p = nullptr;
        BindingList      m_binding_list;
        BindingFlagsList m_binding_flags_list;
        vk::DescriptorSetLayoutCreateFlags m_flags;
        vkenums::DescriptorSetStrategy m_strategy = vkenums::DescriptorSetStrategy::ePerFrame;
        uint32_t m_layout_index = 0u;
        vk::UniqueDescriptorSetLayout m_layout;
    };

}
