#include "vk_core/descriptor_set/buffer/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/memory/entry.h"
#include <array>

namespace lcf::vkc::entry {

void register_descriptor_buffer(DeviceExtensionManifest & manifest) noexcept
{
    register_buffer_device_address(manifest);
    static constexpr std::array k_extensions { vk::EXTDescriptorBufferExtensionName };
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceDescriptorBufferFeaturesEXT::descriptorBuffer),
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceVulkan13Features::synchronization2),
    };
    manifest.addRequiredExtensions(k_extensions)
        .addRequiredFeatures(k_features);
}

} // namespace lcf::vkc::entry
