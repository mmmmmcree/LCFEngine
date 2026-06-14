#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/error.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;
using namespace lcf::vkc::bs;

bool physical_device_filter(vk::PhysicalDevice physical_device, const PhysicalDeviceSelectInfo & info);

uint64_t calc_physical_device_score(vk::PhysicalDevice physical_device);

vk::PhysicalDevice select_physical_device_maythrow(vk::Instance instance, const PhysicalDeviceSelectInfo & info);

} // namespace

namespace lcf::vkc::bs {

std::expected<vk::PhysicalDevice, std::error_code> select_physical_device(vk::Instance instance, const PhysicalDeviceSelectInfo & info) noexcept
{
    vk::PhysicalDevice picked{};
    try {
        picked = select_physical_device_maythrow(instance, info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    if (not picked) {
        return std::unexpected(make_error_code(errc::no_suitable_physical_device));
    }
    return picked;
}

} // namespace lcf::vkc::bs

namespace {

bool physical_device_filter(vk::PhysicalDevice physical_device, const PhysicalDeviceSelectInfo & info)
{
    if (not info.isRequiredFeaturesSupported(physical_device)) { return false; }
    auto supported_extension_count = stdr::count_if(physical_device.enumerateDeviceExtensionProperties(), [&info](const vk::ExtensionProperties & extension) {
        return info.isExtensionRequired(extension.extensionName); 
    });
    if (supported_extension_count != info.getRequiredDeviceExtensionCount()) { return false; }
    return stdr::any_of(physical_device.getQueueFamilyProperties2(), [&](const vk::QueueFamilyProperties2 & props) {
        return (props.queueFamilyProperties.queueFlags & info.getRequiredQueueFlags()) == info.getRequiredQueueFlags();
    });

}

uint64_t calc_physical_device_score(vk::PhysicalDevice physical_device)
{
    const auto & props = physical_device.getProperties();
    uint64_t score = 0;
    switch (props.deviceType) {
        case vk::PhysicalDeviceType::eDiscreteGpu: { score += 1000; } break; 
        case vk::PhysicalDeviceType::eIntegratedGpu: { score += 500; } break;
        case vk::PhysicalDeviceType::eVirtualGpu: { score += 250; } break;
        case vk::PhysicalDeviceType::eCpu: { score += 100; } break;
        default: break;
    }
    const auto & mem_props = physical_device.getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
        const auto & mem_heap = mem_props.memoryHeaps[i];
        if (mem_heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
            score += mem_heap.size >> 20;
        }
    }
    return score;
}

vk::PhysicalDevice select_physical_device_maythrow(vk::Instance instance, const PhysicalDeviceSelectInfo & info)
{
    struct Candidate
    {
        vk::PhysicalDevice physical_device;
        vk::PhysicalDeviceType type;
        uint64_t score;
    };
    std::vector<Candidate> candidates;
    for (const auto & physical_device : instance.enumeratePhysicalDevices()) {
        if (not physical_device_filter(physical_device, info)) { continue; }
        candidates.emplace_back(physical_device, physical_device.getProperties().deviceType, calc_physical_device_score(physical_device));
    }
    if (candidates.empty()) { return {}; }
    auto best_candidate = stdr::max_element(candidates, {}, [&](const Candidate & candidate) {
        const auto & preferred_type_opt = info.getPreferredTypeOptional();
        if (preferred_type_opt.has_value()) {
            return std::make_pair(candidate.type == preferred_type_opt.value(), candidate.score);
        }
        return std::make_pair(false, candidate.score);
    });
    return best_candidate->physical_device;
}

} // namespace