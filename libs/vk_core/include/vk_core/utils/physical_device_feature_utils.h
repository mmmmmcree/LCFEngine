#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/utils/DynamicStructureChain.h"
#include "type_traits/member_pointer_traits.h"

namespace lcf::vkc::utils {

namespace details {

template <typename T>
concept feature_type_c = struct_extends_c<T, vk::PhysicalDeviceFeatures2>;

} // namespace details

class PhysicalDeviceFeatureChain
{
    using Self = PhysicalDeviceFeatureChain;
    using Chain = DynamicStructureChain<vk::PhysicalDeviceFeatures2>;
public:
    const vk::PhysicalDeviceFeatures2 & root() const noexcept { return m_chain.root(); }
    template <details::feature_type_c Feature>
    const Feature & get() const { return m_chain.get<Feature>(); }
    template <details::feature_type_c Feature>
    const Feature * tryGet() const noexcept { return m_chain.tryGet<Feature>(); }
    template <details::feature_type_c Feature>
    Feature & request() noexcept { return m_chain.request<Feature>(); }
    void queryFrom(vk::PhysicalDevice physical_device) noexcept { physical_device.getFeatures2(&m_chain.root()); }
private:
    Chain m_chain;
};

struct PhysicalDeviceFeatureBit
{
    void (*enable)(PhysicalDeviceFeatureChain &);
    bool (*test)(const PhysicalDeviceFeatureChain &);
    const char * name;
};

template <auto k_member>
inline constexpr PhysicalDeviceFeatureBit t_feature_bit {
    .enable = [](PhysicalDeviceFeatureChain & chain) {
        chain.request<typename member_pointer_traits<decltype(k_member)>::class_type>().*k_member = true;
    },
    .test = [](const PhysicalDeviceFeatureChain & queried) {
        using Feature = typename member_pointer_traits<decltype(k_member)>::class_type;
        const Feature * feature_p = queried.tryGet<Feature>();
        return feature_p and static_cast<bool>((*feature_p).*k_member);
    },
    .name = nullptr,
};

} // namespace lcf::vkc::utils

//todo wait for c++26 reflection
#define LCF_VKC_UTILS_FEATURE_BIT(member) \
    ::lcf::vkc::utils::PhysicalDeviceFeatureBit { \
        ::lcf::vkc::utils::t_feature_bit<member>.enable, \
        ::lcf::vkc::utils::t_feature_bit<member>.test, \
        #member, \
    }
