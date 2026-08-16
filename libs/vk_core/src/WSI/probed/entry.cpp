#include "vk_core/WSI/probed/entry.h"
#include <array>

namespace {

using namespace lcf::vkc;

void register_default_swapchain(
    InstanceExtensionManifest & instance_manifest,
    DeviceExtensionManifest & device_manifest) noexcept
{
    entry::register_surface(instance_manifest);
    entry::register_swapchain(instance_manifest, device_manifest);
}

void register_compat_swapchain(
    InstanceExtensionManifest & instance_manifest,
    DeviceExtensionManifest & device_manifest) noexcept
{
    entry::register_surface(instance_manifest);
    entry::register_compat_swapchain(device_manifest);
}

constexpr std::array k_swapchain_options
{
    probe::CapabilityOption {
        .type_header = "vk_core/WSI/probed/default_wapchain.h",
        .register_requirements = &register_default_swapchain,
    },
    probe::CapabilityOption {
        .type_header = "vk_core/WSI/probed/compat_swapchain.h",
        .register_requirements = &register_compat_swapchain,
    },
};

const probe::CapabilityDescriptor k_swapchain_capability = k_swapchain_options;

} // namespace

namespace lcf::vkc::wsi {

const probe::CapabilityDescriptor & capability_descriptor(SwapchainCapability) noexcept
{
    return k_swapchain_capability;
}

} // namespace lcf::vkc::wsi
