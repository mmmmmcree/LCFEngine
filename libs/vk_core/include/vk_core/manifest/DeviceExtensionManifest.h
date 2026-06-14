#pragma once

#include <vulkan/vulkan.hpp>
#include <unordered_set>
#include "vk_core/utils/physical_device_feature_utils.h"
#include "concepts/range_concept.h"

namespace lcf::vkc {

class DeviceExtensionManifest
{
    using Self = DeviceExtensionManifest;
    using StringSet = std::unordered_set<std::string>;
    using PhysicalDeviceFeatureBitList = std::vector<utils::PhysicalDeviceFeatureBit>;
public:
    Self & addRequiredExtension(std::string_view extension_name) noexcept
    {
        m_required_extensions.insert(std::string(extension_name));
        return *this;
    }
    Self & addRequiredExtensions(convertible_range_of_c<std::string> auto && extension_names) noexcept
    {
        m_required_extensions.insert_range(std::forward<decltype(extension_names)>(extension_names));
        return *this;
    }
    Self & addRequiredFeature(utils::PhysicalDeviceFeatureBit feature) noexcept
    {
        feature.enable(m_feature_chain);
        m_required_features.emplace_back(std::move(feature));
        return *this;
    }
    Self & addRequiredFeatures(convertible_range_of_c<utils::PhysicalDeviceFeatureBit> auto && features) noexcept
    {
        for (auto & feature : features) { feature.enable(m_feature_chain); }
        m_required_features.append_range(std::forward<decltype(features)>(features));
        return *this;
    }
    bool isExtensionRequired(const std::string & extension_name) const noexcept
    {
        return m_required_extensions.contains(extension_name);
    }
    const vk::PhysicalDeviceFeatures2 & getRequiredFeatures() const noexcept { return m_feature_chain.root(); }
    bool isRequiredFeaturesSupported(vk::PhysicalDevice physical_device) const noexcept;
private:
    StringSet m_required_extensions;
    utils::PhysicalDeviceFeatureChain m_feature_chain;
    PhysicalDeviceFeatureBitList m_required_features;    
};

} // namespace lcf::vkc
