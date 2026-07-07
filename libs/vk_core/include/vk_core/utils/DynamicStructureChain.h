#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <utility>

namespace lcf::vkc::utils {

namespace details {

template <typename T, typename Root>
concept struct_extends_c = static_cast<bool>(vk::StructExtends<T, Root>::value);

} // namespace details

template <typename Root>
class DynamicStructureChain
{
    using Self = DynamicStructureChain<Root>;
    using NodeMap = std::unordered_map<std::type_index, std::any>;
public:
    DynamicStructureChain() { m_nodes.try_emplace(typeid(Root), Root{}); }
public:
    const Root & root() const noexcept { return std::any_cast<const Root &>(m_nodes.at(typeid(Root))); }
    Root & root() noexcept { return std::any_cast<Root &>(m_nodes.at(typeid(Root))); }
    template <details::struct_extends_c<Root> Extension>
    const Extension & get() const { return std::any_cast<const Extension &>(m_nodes.at(typeid(Extension))); }
    template <details::struct_extends_c<Root> Extension>
    const Extension * tryGet() const noexcept
    {
        auto it = m_nodes.find(typeid(Extension));
        return it == m_nodes.end() ? nullptr : &std::any_cast<const Extension &>(it->second);
    }
    template <details::struct_extends_c<Root> Extension>
    Extension & request() noexcept
    {
        auto [it, inserted] = m_nodes.try_emplace(typeid(Extension), Extension {});
        auto & extension = std::any_cast<Extension &>(it->second);
        if (inserted) { extension.pNext = std::exchange(this->root().pNext, &extension); }
        return extension;
    }
private:
    NodeMap m_nodes;
};

} // namespace lcf::vkc::utils
