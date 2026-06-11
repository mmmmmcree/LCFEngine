#pragma once

#include "type_traits/member_pointer_traits.h"
#include <vulkan/vulkan.hpp>
#include <any>
#include <span>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace lcf::vkc::conf {

template <typename T>
concept feature_struct_c = vk::StructExtends<T, vk::PhysicalDeviceFeatures2>::value;

class FeatureChain
{
    using Root = vk::PhysicalDeviceFeatures2;
    using NodeMap = std::unordered_map<std::type_index, std::any>; // node-based steady map
public:
    FeatureChain() { m_nodes.try_emplace(typeid(Root), Root{}); }
public:
    template <feature_struct_c FeatureStruct>
    FeatureStruct & request()
    {
        auto [it, inserted] = m_nodes.try_emplace(typeid(FeatureStruct), FeatureStruct{});
        auto & feature = std::any_cast<FeatureStruct &>(it->second);
        if (inserted) { feature.pNext = std::exchange(this->root().pNext, &feature); }
        return feature;
    }
    template <feature_struct_c FeatureStruct>
    const FeatureStruct & get() const
    {
        // precondition: the same feature was requested while building this chain
        return std::any_cast<const FeatureStruct &>(m_nodes.at(typeid(FeatureStruct)));
    }
    void queryFrom(vk::PhysicalDevice physical_device) { physical_device.getFeatures2(&this->root()); }
    const Root & root() const noexcept { return std::any_cast<const Root &>(m_nodes.at(typeid(Root))); }
private:
    Root & root() noexcept { return std::any_cast<Root &>(m_nodes.at(typeid(Root))); }
private:
    NodeMap m_nodes;
};

struct FeatureBit
{
    void (*enable)(FeatureChain &);
    bool (*test)(const FeatureChain & queried);
};

template <auto k_member>
inline constexpr FeatureBit k_feature {
    .enable = [](FeatureChain & chain) {
        chain.request<typename member_pointer_traits<decltype(k_member)>::class_type>().*k_member = true;
    },
    .test = [](const FeatureChain & queried) {
        return bool(queried.get<typename member_pointer_traits<decltype(k_member)>::class_type>().*k_member);
    },
};

struct FeatureDependency
{
    uint32_t core_since = 0;
    std::span<const char * const> instance_extensions = {};
    std::span<const char * const> device_extensions = {};
    std::span<const FeatureBit> features = {};
    std::span<const FeatureDependency * const> optional = {};
};

} // namespace lcf::vkc::conf
