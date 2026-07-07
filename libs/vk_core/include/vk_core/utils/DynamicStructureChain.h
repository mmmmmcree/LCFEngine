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
    struct Node
    {
        std::any value;
        void * (*address)(std::any &) noexcept = nullptr;
    };
    using NodeMap = std::unordered_map<std::type_index, Node>;
public:
    ~DynamicStructureChain() noexcept = default;
    DynamicStructureChain() { this->emplaceNode<Root>(); }
    DynamicStructureChain(const Self & other) : m_nodes(other.m_nodes) { this->relink(); }
    DynamicStructureChain(Self && other) noexcept = default;
    Self & operator=(const Self & other)
    {
        if (this == &other) { return *this; }
        m_nodes = other.m_nodes;
        this->relink();
        return *this;
    }
    Self & operator=(Self && other) noexcept = default;
public:
    const Root & root() const noexcept { return std::any_cast<const Root &>(m_nodes.at(typeid(Root)).value); }
    Root & root() noexcept { return std::any_cast<Root &>(m_nodes.at(typeid(Root)).value); }
    template <details::struct_extends_c<Root> Extension>
    const Extension & get() const { return std::any_cast<const Extension &>(m_nodes.at(typeid(Extension)).value); }
    template <details::struct_extends_c<Root> Extension>
    const Extension * tryGet() const noexcept
    {
        auto it = m_nodes.find(typeid(Extension));
        return it == m_nodes.end() ? nullptr : &std::any_cast<const Extension &>(it->second.value);
    }
    template <details::struct_extends_c<Root> Extension>
    Extension & request() noexcept
    {
        auto it = m_nodes.find(typeid(Extension));
        if (it != m_nodes.end()) { return std::any_cast<Extension &>(it->second.value); }
        auto & extension = std::any_cast<Extension &>(this->emplaceNode<Extension>().value);
        extension.pNext = std::exchange(this->root().pNext, &extension);
        return extension;
    }
private:
    template <typename T>
    Node & emplaceNode() noexcept
    {
        constexpr auto address = [](std::any & value) noexcept -> void * { return &std::any_cast<T &>(value); };
        auto [it, _] = m_nodes.try_emplace(typeid(T), Node { std::any { T {} }, address });
        return it->second;
    }
    void relink() noexcept
    {
        auto as_base = [](Node & node) noexcept { return reinterpret_cast<vk::BaseOutStructure *>(node.address(node.value)); };
        std::type_index root_type_index = typeid(Root);
        vk::BaseOutStructure * root_base = as_base(m_nodes.at(root_type_index));
        root_base->pNext = nullptr;
        for (auto & [type, node] : m_nodes) {
            if (type == root_type_index) { continue; }
            vk::BaseOutStructure * base = as_base(node);
            base->pNext = std::exchange(root_base->pNext, base);
        }
    }
private:
    NodeMap m_nodes;
};

} // namespace lcf::vkc::utils
