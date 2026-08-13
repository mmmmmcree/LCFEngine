#include "vk_core/bootstrap/info_structs.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include <format>
#include <algorithm>
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::bs {

bool InstanceCreateInfo::isExtensionRequired(const std::string &extension_name) const noexcept
{
    if (not m_extension_manifest_p) { return false; }
    return m_extension_manifest_p->isExtensionRequired(extension_name);
}

std::size_t InstanceCreateInfo::getRequiredInstanceExtensionCount() const noexcept
{
    if (not m_extension_manifest_p) { return 0; }
    return m_extension_manifest_p->getRequiredExtensionCount();
}

auto InstanceCreateInfo::getExtensionEnableCallback() const noexcept -> ExtEnableCallback
{
    if (not m_extension_manifest_p) { return {}; }
    return [manifest_p = m_extension_manifest_p](vk::Instance instance) { return manifest_p->enableExtensions(instance); };
}

std::string InstanceCreateInfo::getUnsupportedExtensionsMessage() const noexcept
{
    if (not m_extension_manifest_p) { return {}; }
    return m_extension_manifest_p->getUnsupportedExtensionsMessage();
}

std::string InstanceCreateInfo::getUnsupportedLayersMessage() const noexcept
{
    std::string message;
    auto supported_layer_props = vk::enumerateInstanceLayerProperties();
    for (const auto & requried_layer_name : m_required_instance_layers) {
        auto found_it = stdr::find_if(supported_layer_props, [&requried_layer_name](const vk::LayerProperties &layer) {
            return std::string(layer.layerName.data()) == requried_layer_name; });
        if (found_it != supported_layer_props.end()) { continue; }
        message += std::format("unsupported instance layer: {}\n", requried_layer_name);
    }
    return message;
}

bool PhysicalDeviceSelectInfo::isRequiredFeaturesSupported(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return false; }
    return m_extension_manifest_p->isRequiredFeaturesSupported(physical_device);
}
bool PhysicalDeviceSelectInfo::isExtensionRequired(const std::string &extension_name) const noexcept
{
    if (not m_extension_manifest_p) { return false; }
    return m_extension_manifest_p->isExtensionRequired(extension_name);
}

std::size_t PhysicalDeviceSelectInfo::getRequiredDeviceExtensionCount() const noexcept
{
    if (not m_extension_manifest_p) { return 0; }
    return m_extension_manifest_p->getRequiredExtensionCount();
}

std::string PhysicalDeviceSelectInfo::getUnsupportedExtensionsMessage(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return {}; }
    return m_extension_manifest_p->getUnsupportedExtensionsMessage(physical_device);
}

std::string PhysicalDeviceSelectInfo::getUnsupportedFeaturesMessage(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return {}; }
    return m_extension_manifest_p->getUnsupportedFeaturesMessage(physical_device);
}

bool DeviceCreateInfo::isExtensionRequired(const std::string &extension_name) const noexcept
{
    if (not m_extension_manifest_p) { return false; }
    return m_extension_manifest_p->isExtensionRequired(extension_name);
}

bool DeviceCreateInfo::isFeatureRequired(const utils::PhysicalDeviceFeatureBit & feature_bit) const noexcept
{
    if (not m_extension_manifest_p) { return false; }
    return m_extension_manifest_p->isFeatureRequired(feature_bit);
}

std::size_t DeviceCreateInfo::getRequiredDeviceExtensionCount() const noexcept
{
    if (not m_extension_manifest_p) { return 0; }
    return m_extension_manifest_p->getRequiredExtensionCount();
}

const vk::PhysicalDeviceFeatures2 * DeviceCreateInfo::getRequiredFeatures() const noexcept
{
    if (not m_extension_manifest_p) { return nullptr; }
    return &m_extension_manifest_p->getRequiredFeatures();
}

std::string DeviceCreateInfo::getUnsupportedExtensionsMessage(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return {}; }
    return m_extension_manifest_p->getUnsupportedExtensionsMessage(physical_device);
}

std::string DeviceCreateInfo::getUnsupportedFeaturesMessage(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return {}; }
    return m_extension_manifest_p->getUnsupportedFeaturesMessage(physical_device);
}

bool DeviceCreateInfo::isRequiredFeaturesSupported(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return true; }
    return m_extension_manifest_p->isRequiredFeaturesSupported(physical_device);
}

}

