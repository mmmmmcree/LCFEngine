#include "vk_core/descriptor_set/heap/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/memory/entry.h"
#include <array>

namespace lcf::vkc::entry {

void register_descriptor_heap(DeviceExtensionManifest & manifest) noexcept
{
    register_buffer_device_address(manifest);
    static constexpr std::array k_extensions { vk::EXTDescriptorHeapExtensionName };
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceDescriptorHeapFeaturesEXT::descriptorHeap),
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceVulkan13Features::synchronization2),
    };
    manifest.addRequiredExtensions(k_extensions)
        .addRequiredFeatures(k_features);
}

void register_descriptor_heap_untyped(DeviceExtensionManifest & manifest) noexcept
{
    register_descriptor_heap(manifest);
    static constexpr std::array k_extensions { vk::KHRShaderUntypedPointersExtensionName };
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceShaderUntypedPointersFeaturesKHR::shaderUntypedPointers),
    };
    manifest.addRequiredExtensions(k_extensions)
        .addRequiredFeatures(k_features);
}

} // namespace lcf::vkc::entry
