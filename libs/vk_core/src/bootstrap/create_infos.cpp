#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include <iostream>
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

void InstanceCreateInfo::printUnsupportedExtensions() const noexcept
{
    if (not m_extension_manifest_p) { return; }
    m_extension_manifest_p->printUnsupportedExtensions();
}

void InstanceCreateInfo::printUnsupportedLayers() const noexcept
{
    auto supported_layer_props = vk::enumerateInstanceLayerProperties();
    for (const auto & requried_layer_name : m_required_instance_layers) {
        auto found_it = stdr::find_if(supported_layer_props, [&requried_layer_name](const vk::LayerProperties &layer) {
            return std::string(layer.layerName.data()) == requried_layer_name; });
        if (found_it != supported_layer_props.end()) { continue; }
        std::cout << std::format("[vkc error] unsupported instance layer: {}\n", requried_layer_name);
    }
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

void DeviceCreateInfo::printUnsupportedExtensions(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return; }
    m_extension_manifest_p->printUnsupportedExtensions(physical_device);
}

void DeviceCreateInfo::printUnsupportedFeatures(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return; }
    m_extension_manifest_p->printUnsupportedFeatures(physical_device);
}

bool DeviceCreateInfo::isRequiredFeaturesSupported(vk::PhysicalDevice physical_device) const noexcept
{
    if (not m_extension_manifest_p) { return true; }
    return m_extension_manifest_p->isRequiredFeaturesSupported(physical_device);
}

}

