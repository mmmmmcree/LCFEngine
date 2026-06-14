#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"

namespace lcf::vkc::bs {

InstanceCreateInfo::~InstanceCreateInfo() noexcept = default;

auto InstanceCreateInfo::setRequiredInstanceExtensionManifest(const InstanceExtensionManifest & extension_manifest) noexcept -> Self &
{
    m_extension_manifest_p = &extension_manifest;
    return *this;
}

bool InstanceCreateInfo::isExtensionRequired(const std::string &extension_name) const noexcept
{
    if (not m_extension_manifest_p) { return false; }
    return m_extension_manifest_p->isExtensionRequired(extension_name);
}

} // namespace lcf::vkc::bs