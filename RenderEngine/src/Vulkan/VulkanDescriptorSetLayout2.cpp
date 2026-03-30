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

    std::vector<vk::DescriptorSetLayoutBinding> raw_bindings;
    raw_bindings.reserve(m_binding_list.size());
    for (auto & b : m_binding_list) {
        raw_bindings.emplace_back(static_cast<vk::DescriptorSetLayoutBinding>(b));
    }

    vk::DescriptorSetLayoutCreateInfo layout_info;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info;

    if (m_strategy == Strategy::eBindless) {
        m_flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
        m_binding_flags_list.resize(m_binding_list.size());
        for (auto & flags : m_binding_flags_list) {
            flags = vk::DescriptorBindingFlagBits::eUpdateAfterBind
                  | vk::DescriptorBindingFlagBits::ePartiallyBound
                  | vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;
        }
        for (size_t i = 0; i < m_binding_list.size(); ++i) {
            if (m_binding_list[i].is_runtime_array) {
                m_binding_flags_list[i] |= vk::DescriptorBindingFlagBits::eVariableDescriptorCount;
                raw_bindings[i].descriptorCount = vkconstants::bindless::k_max_variable_descriptor_count;
            }
        }
        binding_flags_info.setBindingFlags(m_binding_flags_list);
        layout_info.setPNext(&binding_flags_info);
    }

    layout_info.setBindings(raw_bindings)
               .setFlags(m_flags);

    m_layout = m_context_p->getDevice().createDescriptorSetLayoutUnique(layout_info);
}
