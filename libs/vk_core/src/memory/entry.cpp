#include "vk_core/memory/entry.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include <array>

namespace lcf::vkc::entry {

void register_memory(InstanceExtensionManifest & manifest) noexcept
{

}

void register_buffer_device_address(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceVulkan12Features::bufferDeviceAddress),
    };
    manifest.addRequiredFeatures(k_features);
}

} // namespace lcf::vkc::entry
