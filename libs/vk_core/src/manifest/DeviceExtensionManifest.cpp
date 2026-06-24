#include "vk_core/manifest/DeviceExtensionManifest.h"
#include <iostream>
#include <format>
#include <algorithm>
#include <ranges>

namespace stdr = std::ranges;

namespace lcf::vkc {

bool DeviceExtensionManifest::isRequiredFeaturesSupported(vk::PhysicalDevice physical_device) const noexcept
{
    utils::PhysicalDeviceFeatureChain feature_chain;
    for (const auto & feature : m_required_features) { feature.enable(feature_chain); }
    feature_chain.queryFrom(physical_device);
    return stdr::all_of(m_required_features, [&feature_chain](const auto & feature) { return feature.test(feature_chain); });
}

void DeviceExtensionManifest::printUnsupportedExtensions(vk::PhysicalDevice physical_device) const noexcept
{
    auto supported_extension_props = physical_device.enumerateDeviceExtensionProperties();
    for (const auto & requried_ext_name : m_required_extensions) {
        auto found_it = stdr::find_if(supported_extension_props, [&requried_ext_name](const vk::ExtensionProperties &props) {
            return std::string(props.extensionName.data()) == requried_ext_name; });
        if (found_it != supported_extension_props.end()) { continue; }
        std::cout << std::format("[vkc error] unsupported device extension: {}\n", requried_ext_name);
    }
}

void DeviceExtensionManifest::printUnsupportedFeatures(vk::PhysicalDevice physical_device) const noexcept
{
    utils::PhysicalDeviceFeatureChain feature_chain;
    for (const auto & feature : m_required_features) { feature.enable(feature_chain); }
    feature_chain.queryFrom(physical_device);
    for (const auto & feature : m_required_features) {
        if (not feature.test(feature_chain)) {
            std::cout << std::format("[vkc error] unsupported device feature: {}\n", feature.name);
        }
    }
}

} // namespace lcf::vkc


