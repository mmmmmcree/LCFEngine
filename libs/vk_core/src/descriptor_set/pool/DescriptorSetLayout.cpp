#include "vk_core/descriptor_set/pool/DescriptorSetLayout.h"
#include "vk_core/descriptor_set/info_structs.h"
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::dsp {

std::error_code DescriptorSetLayout::create(vk::Device device, const DescriptorSetLayoutInfo &info) noexcept
{
    const auto & bindings = info.getBindings();
    if (bindings.empty()) { return {}; }
    const auto & binding_flags_list = info.getBindingFlags();
    vk::DescriptorBindingFlags binding_flags = {};
    for (const auto & flags : binding_flags_list) { binding_flags |= flags; }
    vk::DescriptorSetLayoutCreateInfo layout_info;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info;
    vk::DescriptorSetLayoutCreateFlags layout_flags {};
    if (binding_flags & vk::DescriptorBindingFlagBits::eUpdateAfterBind) {
        layout_flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    }
    binding_flags_info.setBindingFlags(binding_flags_list);
    layout_info.setBindings(bindings)
        .setFlags(layout_flags)
        .setPNext(&binding_flags_info);
    try {
        vk::UniqueDescriptorSetLayout layout = device.createDescriptorSetLayoutUnique(layout_info);
        m_layout = std::move(layout);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    m_flags = binding_flags;
    m_bindings = bindings;
    return {};
}

} // namespace lcf::vkc::dsp

