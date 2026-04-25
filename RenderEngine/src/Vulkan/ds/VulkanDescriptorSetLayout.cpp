#include "Vulkan/ds/VulkanDescriptorSetLayout.h"
#include "Vulkan/vulkan_constants.h"
#include <ranges>

using namespace lcf::render;
namespace stdv = std::views;

VulkanDescriptorSetLayout & VulkanDescriptorSetLayout::setBindings(BindingReadSpan bindings) noexcept
{
    m_bindings.assignRange(bindings);
    return *this;
}

 VulkanDescriptorSetLayout & VulkanDescriptorSetLayout::addBinding(const VulkanDescriptorSetBinding &binding) noexcept
{
    m_bindings.pushBack(binding);
    return *this;
}

VulkanDescriptorSetLayout & VulkanDescriptorSetLayout::setIndex(uint32_t index) noexcept
{
    m_layout_index = index;
    return *this;
}

std::error_code VulkanDescriptorSetLayout::create(vk::Device device, vkenums::DescriptorSetStrategy strategy) noexcept
{
    m_strategy = strategy;
    if (m_bindings.empty()) {
        try {
            m_layout = device.createDescriptorSetLayoutUnique({});
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
