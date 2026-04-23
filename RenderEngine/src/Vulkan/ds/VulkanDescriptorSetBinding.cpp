#include "Vulkan/ds/VulkanDescriptorSetBinding.h"
#include <ranges>
#include <algorithm>

using namespace lcf::render;
namespace stdr = std::ranges;

bool VulkanDescriptorSetBinding::operator==(const Self &other) const noexcept
{
    return m_flags == other.m_flags and m_binding == other.m_binding;
}

bool VulkanDescriptorSetLayoutBindings::operator==(const Self &other) const noexcept
{
    return stdr::equal(m_bindings, other.m_bindings);
}
