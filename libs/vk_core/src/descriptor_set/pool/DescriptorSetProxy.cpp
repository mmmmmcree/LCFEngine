#include "vk_core/descriptor_set/pool/DescriptorSetProxy.h"

namespace lcf::vkc::dsp {

std::error_code DescriptorSetProxy::create(DescriptorSetAllocator & allocator, const DescriptorSetLayout & layout) noexcept
{
    if  (not layout.handle()) { return std::make_error_code(std::errc::invalid_argument); }
    m_allocator_p = &allocator;
    m_layout = layout;
    m_authority_bindings.reserve(layout.getBindings().size());
    for (const auto & binding : m_layout.getBindings()) {
        auto & bindings = m_authority_bindings.emplace_back();
        bindings.resize(binding.descriptorCount);
    }
    return {};
}

} // namespace lcf::vkc::dsp

