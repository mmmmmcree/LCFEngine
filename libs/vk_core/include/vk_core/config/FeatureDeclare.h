#pragma once

#include <vulkan/vulkan.hpp>
#include <algorithm>
#include <any>
#include <deque>
#include <span>
#include <typeindex>
#include <utility>

namespace lcf::vkc::conf {

// runtime counterpart of vk::StructureChain: the struct set is determined by
// which dependencies the caller passes to bootstrap, which a compile-time chain cannot host
class FeatureChain
{
public:
    template <typename FeatureStruct>
    FeatureStruct & request()
    {
        if (void * existing_p = this->find(typeid(FeatureStruct))) {
            return *static_cast<FeatureStruct *>(existing_p);
        }
        auto & node = m_nodes.emplace_back(FeatureStruct{});
        auto & feature = std::any_cast<FeatureStruct &>(node.storage);
        node.value_p = &feature;
        feature.pNext = std::exchange(m_root.pNext, &feature);
        return feature;
    }

    template <typename FeatureStruct>
    const FeatureStruct & get() const
    {
        // precondition: some dependency requested FeatureStruct before the query
        return *static_cast<const FeatureStruct *>(this->find(typeid(FeatureStruct)));
    }

    vk::PhysicalDeviceFeatures2 & root() noexcept { return m_root; }
    const vk::PhysicalDeviceFeatures2 & root() const noexcept { return m_root; }

private:
    struct Node
    {
        template <typename FeatureStruct>
        explicit Node(FeatureStruct feature) : storage(std::move(feature)), type(typeid(FeatureStruct)) {}
        std::any storage;
        std::type_index type;
        void * value_p = nullptr;
    };

    void * find(std::type_index type) const noexcept
    {
        auto it = std::ranges::find(m_nodes, type, &Node::type);
        return it == m_nodes.end() ? nullptr : it->value_p;
    }

    vk::PhysicalDeviceFeatures2 m_root;
    std::deque<Node> m_nodes;
};

template <typename>
struct MemberPointerTraits;

template <typename Value, typename Class>
struct MemberPointerTraits<Value Class::*>
{
    using class_type = Class;
};

// one boolean member of a vk feature struct that must be set to true
struct FeatureBit
{
    void (*enable)(FeatureChain &);
    bool (*test)(const FeatureChain & queried);
};

template <auto k_member>
inline constexpr FeatureBit k_feature {
    .enable = [](FeatureChain & chain) {
        using FeatureStruct = typename MemberPointerTraits<decltype(k_member)>::class_type;
        chain.request<FeatureStruct>().*k_member = true;
    },
    .test = [](const FeatureChain & queried) {
        using FeatureStruct = typename MemberPointerTraits<decltype(k_member)>::class_type;
        return bool(queried.get<FeatureStruct>().*k_member);
    },
};

// one k_module_dependency per module, declared in <module>/feature_dependencies.h:
// everything the module's classes unconditionally require; nice-to-have capabilities
// are separate k_xxx_dependency objects hung off `optional` and never gate the module
struct FeatureDependency
{
    uint32_t core_since = 0;
    std::span<const char * const> instance_extensions = {};
    std::span<const char * const> device_extensions = {};
    std::span<const FeatureBit> features = {};
    std::span<const FeatureDependency * const> optional = {};
};

} // namespace lcf::vkc::conf
