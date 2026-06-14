#include "vk_core/manifest/DeviceExtensionManifest.h"

namespace stdr = std::ranges;

namespace lcf::vkc {

bool DeviceExtensionManifest::isRequiredFeaturesSupported(vk::PhysicalDevice physical_device) const noexcept
{
    utils::PhysicalDeviceFeatureChain feature_chain;
    for (const auto & feature : m_required_features) { feature.enable(feature_chain); }
    feature_chain.queryFrom(physical_device);
    return stdr::all_of(m_required_features, [&feature_chain](const auto & feature) { return feature.test(feature_chain); });
}

} // namespace lcf::vkc


