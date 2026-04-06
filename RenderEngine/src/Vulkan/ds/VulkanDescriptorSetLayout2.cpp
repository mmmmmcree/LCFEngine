#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/vulkan_constants.h"
#include <ranges>

using namespace lcf::render;
namespace stdv = std::views;

VulkanDescriptorSetLayout2 & VulkanDescriptorSetLayout2::setBindings(BindingReadSpan bindings) noexcept
{
    m_binding_list.assign_range(bindings);
    return *this;
}

VulkanDescriptorSetLayout2 & VulkanDescriptorSetLayout2::setIndex(uint32_t index) noexcept
{
    m_layout_index = index;
    return *this;
}

std::error_code VulkanDescriptorSetLayout2::create(vk::Device device, vkenums::DescriptorSetStrategy strategy) noexcept
{
    if (m_binding_list.empty()) { return std::make_error_code(std::errc::invalid_argument); }
    m_strategy = strategy;
    std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;
    std::vector<vk::DescriptorBindingFlags> layout_binding_flags;
    layout_bindings.reserve(m_binding_list.size());
    layout_binding_flags.reserve(m_binding_list.size());
    
    layout_bindings.assign_range(m_binding_list | stdv::transform([](const auto & binding) { return binding.getLayoutBinding(); }));
    layout_binding_flags.assign_range(m_binding_list | stdv::transform([](const auto & binding) { return binding.getFlags(); }));

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
