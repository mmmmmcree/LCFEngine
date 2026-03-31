#include "Vulkan/VulkanDescriptorSetLayout2.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_constants.h"

using namespace lcf::render;
using Strategy = vkenums::DescriptorSetStrategy;

VulkanDescriptorSetLayout2 & VulkanDescriptorSetLayout2::setBindings(BindingReadSpan bindings)
{
    m_binding_list.assign(bindings.begin(), bindings.end());
    return *this;
}

VulkanDescriptorSetLayout2 & VulkanDescriptorSetLayout2::setIndex(uint32_t index) noexcept
{
    m_layout_index = index;
    return *this;
}

VulkanDescriptorSetLayout2 & VulkanDescriptorSetLayout2::setStrategy(Strategy strategy) noexcept
{
    m_strategy = strategy;
    return *this;
}

void VulkanDescriptorSetLayout2::create(VulkanContext * context_p)
{
    m_context_p = context_p;

    // --- extract raw bindings and collect per-binding flags ---
    std::vector<vk::DescriptorSetLayoutBinding> raw_bindings;
    raw_bindings.reserve(m_binding_list.size());

    m_binding_flags_list.resize(m_binding_list.size());

    constexpr auto kBindlessBaseFlags = vk::DescriptorBindingFlagBits::eUpdateAfterBind
                                      | vk::DescriptorBindingFlagBits::ePartiallyBound
                                      | vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;

    for (size_t i = 0; i < m_binding_list.size(); ++i) {
        auto & b = m_binding_list[i];

        // If strategy is bindless, ensure base bindless flags are present on every binding
        if (m_strategy == Strategy::eBindless) {
            b.flags |= kBindlessBaseFlags;
        }

        // If binding has eVariableDescriptorCount, set descriptorCount to layout max
        if (b.hasFlag(vk::DescriptorBindingFlagBits::eVariableDescriptorCount)) {
            b.descriptorCount = vkconstants::bindless::k_max_variable_descriptor_count;
        }

        m_binding_flags_list[i] = b.flags;
        raw_bindings.emplace_back(static_cast<vk::DescriptorSetLayoutBinding>(b));
    }

    // --- create layout ---
    vk::DescriptorSetLayoutCreateInfo layout_info;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info;

    bool has_any_flags = std::ranges::any_of(m_binding_flags_list, [](auto f) {
        return f != vk::DescriptorBindingFlags{};
    });

    if (has_any_flags) {
        if (m_strategy == Strategy::eBindless) {
            m_flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
        }
        binding_flags_info.setBindingFlags(m_binding_flags_list);
        layout_info.setPNext(&binding_flags_info);
    }

    layout_info.setBindings(raw_bindings)
               .setFlags(m_flags);

    m_layout = m_context_p->getDevice().createDescriptorSetLayoutUnique(layout_info);
}
