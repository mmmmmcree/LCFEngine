#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <utility>
#include "vk_core/utils/concepts/structure_concepts.h"

namespace lcf::vkc::utils {

template <typename Root, typename ExtensionRoot = Root, typename... ExtensionRoots>
class DynamicStructureChain
{
    using Self = DynamicStructureChain<Root, ExtensionRoot, ExtensionRoots...>;
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
    template <typename Extension>
    requires struct_extends_any_c<Extension, ExtensionRoot, ExtensionRoots...>
    const Extension & get() const { return std::any_cast<const Extension &>(m_nodes.at(typeid(Extension)).value); }
    template <typename Extension>
    requires struct_extends_any_c<Extension, ExtensionRoot, ExtensionRoots...>
    const Extension * tryGet() const noexcept
    {
        auto it = m_nodes.find(typeid(Extension));
        return it == m_nodes.end() ? nullptr : &std::any_cast<const Extension &>(it->second.value);
    }
    template <typename Extension>
    requires struct_extends_any_c<Extension, ExtensionRoot, ExtensionRoots...>
    Extension & request() noexcept
    {
        auto it = m_nodes.find(typeid(Extension));
        if (it != m_nodes.end()) { return std::any_cast<Extension &>(it->second.value); }
        auto & extension = std::any_cast<Extension &>(this->emplaceNode<Extension>().value);
        auto * extension_base = reinterpret_cast<vk::BaseOutStructure *>(&extension);
        extension.pNext = std::exchange(this->root().pNext, extension_base);
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
        for (auto && [type, node] : m_nodes) {
            if (type == root_type_index) { continue; }
            vk::BaseOutStructure * base = as_base(node);
            base->pNext = std::exchange(root_base->pNext, base);
        }
    }
private:
    NodeMap m_nodes;
};

} // namespace lcf::vkc::utils
