#include "vk_core/sync/entry.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include <array>

namespace lcf::vkc::sync {

void register_sync_module(InstanceExtensionManifest & manifest) noexcept
{

}

void register_timeline_semaphore(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_features
    {
        utils::t_feature_bit<&vk::PhysicalDeviceVulkan12Features::timelineSemaphore>,
    };
    manifest.addRequiredFeatures(k_features);
}

} // namespace lcf::vkc::sync