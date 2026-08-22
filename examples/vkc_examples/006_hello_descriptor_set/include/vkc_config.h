#pragma once

#include "vk_core/probe/probe.h"
#include "vk_core/WSI/probed/entry.h"

namespace lcf::vkc::probe {

inline void configure_physical_device(
    bs::PhysicalDeviceSelectInfo & info) noexcept
{
    info.setPreferredType(vk::PhysicalDeviceType::eDiscreteGpu);
}

inline void register_capabilities(CapabilityRegistry & capabilities)
{
    capabilities.require<wsi::SwapchainCapability>();
}

} // namespace lcf::vkc::probe
