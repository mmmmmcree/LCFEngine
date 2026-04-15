#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/vulkan_constants.h"
#include <ranges>

using namespace lcf::render;
namespace stdv = std::views;

VulkanDescriptorSetLayout2 & VulkanDescriptorSetLayout2::setBindings(BindingReadSpan bindings) noexcept
{
    m_bindings.assign_range(bindings);
    return *this;
}

VulkanDescriptorSetLayout2 & VulkanDescriptorSetLayout2::setIndex(uint32_t index) noexcept
{
    m_layout_index = index;
    return *this;
}

std::error_code VulkanDescriptorSetLayout2::create(vk::Device device, vkenums::DescriptorSetStrategy strategy) noexcept
{
    m_strategy = strategy;
    if (m_bindings.empty()) {
        vk::DescriptorSetLayoutCreateInfo empty_info;
        try {
            m_layout = device.createDescriptorSetLayoutUnique(empty_info);
        } catch (const vk::SystemError & e) {
            return e.code();
        }
        return {};
    }
    std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;
    std::vector<vk::DescriptorBindingFlags> layout_binding_flags;
    layout_bindings.reserve(m_bindings.size());
    layout_binding_flags.reserve(m_bindings.size());

    layout_bindings.assign_range(m_bindings | stdv::transform([](const auto & binding) { return binding.getLayoutBinding(); }));
    layout_binding_flags.assign_range(m_bindings | stdv::transform([](const auto & binding) { return binding.getFlags(); }));

    vk::DescriptorSetLayoutCreateInfo layout_info;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info;
    vk::DescriptorSetLayoutCreateFlags layout_flags {};
    if (m_strategy == vkenums::DescriptorSetStrategy::eBindless) {
        layout_flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
        // magic number detection: last binding descriptorCount == 0 means runtime array
        if (layout_bindings.back().descriptorCount == 0) {
            layout_bindings.back().descriptorCount = vkconstants::ds::k_max_variable_descriptor_count;
            std::ranges::fill(layout_binding_flags,
                vk::DescriptorBindingFlagBits::ePartiallyBound |
                vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending);
            layout_binding_flags.back() = vk::FlagTraits<vk::DescriptorBindingFlagBits>::allFlags;
        }
    }
    binding_flags_info.setBindingFlags(layout_binding_flags);
    layout_info.setBindings(layout_bindings)
        .setFlags(layout_flags)
        .setPNext(&binding_flags_info);
    try {
        m_layout = device.createDescriptorSetLayoutUnique(layout_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}
