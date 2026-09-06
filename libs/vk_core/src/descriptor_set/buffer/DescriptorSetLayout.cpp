#include "vk_core/descriptor_set/buffer/DescriptorSetLayout.h"
#include "vk_core/descriptor_set/info_structs.h"

namespace lcf::vkc::dsb {

std::error_code DescriptorSetLayout::create(vk::Device device, const DescriptorSetLayoutInfo & info) noexcept
{
    const auto & bindings = info.getBindings();
    if (bindings.empty()) { return {}; }
    const auto & binding_flags_list = info.getBindingFlags();
    vk::DescriptorBindingFlags binding_flags = {};
    for (const auto & flags : binding_flags_list) { binding_flags |= flags; }
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info;
    binding_flags_info.setBindingFlags(binding_flags_list);
    vk::DescriptorSetLayoutCreateFlags layout_flags = info.getLayoutFlags() | vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT;
    if (binding_flags & vk::DescriptorBindingFlagBits::eUpdateAfterBind) {
        layout_flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    }
    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.setBindings(bindings)
        .setFlags(layout_flags)
        .setPNext(&binding_flags_info);
    vk::UniqueDescriptorSetLayout layout;
    try {
        layout = device.createDescriptorSetLayoutUnique(layout_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    LayoutOffsetList layout_offsets;
    layout_offsets.reserve(bindings.size());
    for (const auto & binding : bindings) {
        layout_offsets.emplace_back(device.getDescriptorSetLayoutBindingOffsetEXT(layout.get(), binding.binding));
    }
    m_layout_size = device.getDescriptorSetLayoutSizeEXT(layout.get());
    m_layout_offsets = std::move(layout_offsets);
    m_layout = std::move(layout);
    m_flags = binding_flags;
    m_bindings = bindings;
    return {};
}

} // namespace lcf::vkc::dsb
