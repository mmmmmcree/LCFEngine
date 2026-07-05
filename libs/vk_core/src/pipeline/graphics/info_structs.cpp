#include "vk_core/pipeline/graphics/info_structs.h"
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

auto VertexInputInfo::genBindingDescriptions2EXT() const -> BindingList2EXT
{
    auto to_2ext = [this](const auto & indexed) -> vk::VertexInputBindingDescription2EXT {
        const auto & [i, binding] = indexed;
        return { binding.binding, binding.stride, binding.inputRate, std::max(1u, m_binding_divisors[i]) };
    };
    return m_bindings | stdv::enumerate | stdv::transform(to_2ext) | stdr::to<BindingList2EXT>();
}

auto VertexInputInfo::genAttributeDescriptions2EXT() const -> AttributeList2EXT
{
    auto to_2ext = [](const vk::VertexInputAttributeDescription & attribute) {
        return vk::VertexInputAttributeDescription2EXT { attribute.location, attribute.binding, attribute.format, attribute.offset };
    };
    return m_attributes | stdv::transform(to_2ext) | stdr::to<AttributeList2EXT>();
}

} // namespace lcf::vkc
