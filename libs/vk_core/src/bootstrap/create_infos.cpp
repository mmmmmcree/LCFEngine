#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"

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

}

