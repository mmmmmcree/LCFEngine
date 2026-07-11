#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/bootstrap/info_structs.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/error.h"
#include <tuple>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;
using namespace lcf::vkc::bs;

bool physical_device_filter(vk::PhysicalDevice physical_device, const PhysicalDeviceSelectInfo & info);

constexpr int physical_device_type_rank(vk::PhysicalDeviceType type) noexcept;

uint64_t get_physical_device_vram_mib(vk::PhysicalDevice physical_device);

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

constexpr int physical_device_type_rank(vk::PhysicalDeviceType type) noexcept
{
    switch (type) {
        case vk::PhysicalDeviceType::eDiscreteGpu: { return 4; }
        case vk::PhysicalDeviceType::eIntegratedGpu: { return 3; }
        case vk::PhysicalDeviceType::eVirtualGpu: { return 2; }
        case vk::PhysicalDeviceType::eCpu: { return 1; }
        case vk::PhysicalDeviceType::eOther: { return 0; }
    }
    return 0;
}

uint64_t get_physical_device_vram_mib(vk::PhysicalDevice physical_device)
{
    const auto & mem_props = physical_device.getMemoryProperties();
    uint64_t vram_mib = 0;
    for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
        const auto & mem_heap = mem_props.memoryHeaps[i];
        if (mem_heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
            vram_mib += mem_heap.size >> 20;
        }
    }
    return vram_mib;
}

vk::PhysicalDevice select_physical_device_maythrow(vk::Instance instance, const PhysicalDeviceSelectInfo & info)
{
    struct Candidate
    {
        vk::PhysicalDevice physical_device;
        vk::PhysicalDeviceType type;
        uint64_t vram_mib;
    };
    std::vector<Candidate> candidates;
    for (const auto & physical_device : instance.enumeratePhysicalDevices()) {
        if (not physical_device_filter(physical_device, info)) { continue; }
        candidates.emplace_back(
            physical_device,
            physical_device.getProperties().deviceType,
            get_physical_device_vram_mib(physical_device));
    }
    if (candidates.empty()) { return {}; }
    const auto & preferred_type_opt = info.getPreferredTypeOptional();
    auto best_candidate = stdr::max_element(candidates, {}, [&](const Candidate & candidate) {
        bool preferred_match = preferred_type_opt.has_value() and candidate.type == preferred_type_opt.value();
        return std::tuple{preferred_match, physical_device_type_rank(candidate.type), candidate.vram_mib};
    });
    return best_candidate->physical_device;
}

} // namespace