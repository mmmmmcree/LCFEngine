#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <any>
#include <typeindex>
#include "type_traits/member_pointer_traits.h"

namespace lcf::vkc::utils {

namespace details {

template <typename T>
concept feature_type_c = static_cast<bool>(vk::StructExtends<T, vk::PhysicalDeviceFeatures2>::value);

} // namespace details

class PhysicalDeviceFeatureChain
{
    using Self = PhysicalDeviceFeatureChain;
    using Root = vk::PhysicalDeviceFeatures2;
    using NodeMap = std::unordered_map<std::type_index, std::any>; // node-based steady map
public:
    PhysicalDeviceFeatureChain() { m_nodes.try_emplace(typeid(Root), Root{}); }
public:
    const Root & root() const noexcept { return std::any_cast<const Root &>(m_nodes.at(typeid(Root))); }
    template <details::feature_type_c Feature>
    const Feature & get() const { return std::any_cast<const Feature &>(m_nodes.at(typeid(Feature))); }
    template <details::feature_type_c Feature>
    Feature & request() noexcept
    {
        auto [it, inserted] = m_nodes.try_emplace(typeid(Feature), Feature {});
        auto & feature = std::any_cast<Feature &>(it->second);
        if (inserted) { feature.pNext = std::exchange(this->root().pNext, &feature); }
        return feature;
    }
    void queryFrom(vk::PhysicalDevice physical_device) noexcept { physical_device.getFeatures2(&this->root()); }
private:
    Root & root() noexcept { return std::any_cast<Root &>(m_nodes.at(typeid(Root))); }
private:
    NodeMap m_nodes;
};

struct PhysicalDeviceFeatureBit
{
    void (*enable)(PhysicalDeviceFeatureChain &);
    bool (*test)(const PhysicalDeviceFeatureChain &);
};

template <auto k_member>
inline constexpr PhysicalDeviceFeatureBit t_feature_bit {
    .enable = [](PhysicalDeviceFeatureChain & chain) {
        chain.request<typename member_pointer_traits<decltype(k_member)>::class_type>().*k_member = true;
    },
    .test = [](const PhysicalDeviceFeatureChain & queried) {
        return static_cast<bool>(queried.get<typename member_pointer_traits<decltype(k_member)>::class_type>().*k_member);
    },
};

} // namespace lcf::vkc::utils