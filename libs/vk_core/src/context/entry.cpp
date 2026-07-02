#include "vk_core/context/entry.h"
#include "vk_core/sync/entry.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"

namespace lcf::vkc {

void register_context_module(InstanceExtensionManifest & inst_manifest, DeviceExtensionManifest & device_manifest) noexcept
{
    register_timeline_semaphore(device_manifest);
}

} // namespace lcf::vkc::sync