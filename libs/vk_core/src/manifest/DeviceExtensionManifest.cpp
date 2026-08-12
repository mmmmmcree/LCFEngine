#include "vk_core/manifest/DeviceExtensionManifest.h"
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

std::string DeviceExtensionManifest::getUnsupportedExtensionsMessage(vk::PhysicalDevice physical_device) const noexcept
{
    std::string device_name = physical_device.getProperties().deviceName.data();
    auto supported_extension_props = physical_device.enumerateDeviceExtensionProperties();
    std::string message;
    for (const auto & required_ext_name : m_required_extensions) {
        auto found_it = stdr::find_if(supported_extension_props, [&required_ext_name](const vk::ExtensionProperties & props) {
            return std::string(props.extensionName.data()) == required_ext_name; });
        if (found_it != supported_extension_props.end()) { continue; }
        message += std::format("unsupported device extension on {}: {}\n", device_name, required_ext_name);
    }
    return message;
}

std::string DeviceExtensionManifest::getUnsupportedFeaturesMessage(vk::PhysicalDevice physical_device) const noexcept
{
    std::string device_name = physical_device.getProperties().deviceName.data();
    utils::PhysicalDeviceFeatureChain feature_chain;
    for (const auto & feature : m_required_features) { feature.enable(feature_chain); }
    feature_chain.queryFrom(physical_device);
    std::string message;
    for (const auto & feature : m_required_features) {
        if (feature.test(feature_chain)) { continue; }
        message += std::format("unsupported device feature on {}: {}\n", device_name, feature.name);
    }
    return message;
}

} // namespace lcf::vkc


