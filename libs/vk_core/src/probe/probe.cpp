#include "vk_core/probe/probe.h"
#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/context/InstanceContext.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include <algorithm>
#include <numeric>
#include <ranges>

namespace stdr = std::ranges;

namespace {

using namespace lcf::vkc;
using namespace lcf::vkc::probe;

using CapabilitySelection = std::vector<std::size_t>;

using CapabilitiesSelectionResult = std::vector<CapabilitySelection>;

using CapabilitiesView = std::span<const CapabilityDescriptor * const>;

void collect_capability_selections(
    CapabilitiesView capabilities,
    std::size_t capability_index,
    CapabilitySelection & current,
    CapabilitiesSelectionResult & selection_result);

CapabilitiesSelectionResult make_capability_selections(CapabilitiesView capabilities);

std::expected<ProbeResult, Error> try_capability_selection(
    const bs::PhysicalDeviceSelectInfo & physical_device_selection,
    CapabilitiesView capabilities,
    const CapabilitySelection & option_selection) noexcept;

} // namespace

namespace lcf::vkc::probe {

ProbeResult::ProbeResult(
    const vk::PhysicalDeviceProperties & physical_device_properties,
    std::span<const CapabilityDescriptor * const> capability_descriptors,
    std::span<const std::size_t> option_selection) :
    name(physical_device_properties.deviceName.data()),
    vendor_id(physical_device_properties.vendorID),
    device_id(physical_device_properties.deviceID),
    api_version(physical_device_properties.apiVersion)
{
    capabilities.reserve(capability_descriptors.size());
    for (std::size_t index = 0; index < capability_descriptors.size(); ++index) {
        capabilities.emplace_back( capability_descriptors[index], option_selection[index]);
    }
}

std::expected<ProbeResult, Error> run(
    const bs::PhysicalDeviceSelectInfo & physical_device_select_info,
    const CapabilityRegistry & capability_registry) noexcept
{
    try {
        std::vector<const CapabilityDescriptor *> capabilities;
        capabilities.append_range(capability_registry.getRequiredCapabilities());
        if (stdr::any_of(capabilities, [](const CapabilityDescriptor * capability) { return capability->empty(); })) {
            return std::unexpected(Error {errc::no_suitable_physical_device, "a required capability has no option"});
        }
        const auto capability_selections = make_capability_selections(capabilities);
        Error error;
        for (const CapabilitySelection & option_selection : capability_selections) {
            auto result = try_capability_selection(physical_device_select_info, capabilities, option_selection);
            if (result) { return std::move(result); }
            error = std::move(result.error());
            if (error.code() != errc::missing_required_instance_extension and error.code() != errc::no_suitable_physical_device) {
                return std::unexpected(std::move(error));
            }
        }
        return std::unexpected(std::move(error));
    } catch (const std::exception & error) {
        return std::unexpected(Error {errc::no_suitable_instance, error.what()});
    }
}

} // namespace lcf::vkc::probe

namespace {

void collect_capability_selections(
    CapabilitiesView capabilities,
    std::size_t capability_index,
    CapabilitySelection & current_selection,
    CapabilitiesSelectionResult & selection_result)
{
    if (capability_index == capabilities.size()) {
        selection_result.emplace_back(current_selection);
        return;
    }
    const std::size_t option_count = capabilities[capability_index]->size();
    for (std::size_t option_index = 0; option_index < option_count; ++option_index) {
        current_selection[capability_index] = option_index;
        collect_capability_selections(capabilities, capability_index + 1, current_selection, selection_result);
    }
}

CapabilitiesSelectionResult make_capability_selections(CapabilitiesView capabilities)
{
    CapabilitiesSelectionResult selection_result;
    CapabilitySelection current_selection(capabilities.size(), 0);
    collect_capability_selections(capabilities, 0, current_selection, selection_result);
    stdr::stable_sort(selection_result, {}, [](const CapabilitySelection & selection) {
        return std::accumulate(selection.begin(), selection.end(), std::size_t {0});
    });
    return selection_result;
}

std::expected<ProbeResult, Error> try_capability_selection(
    const bs::PhysicalDeviceSelectInfo & physical_device_select_info,
    CapabilitiesView capabilities,
    const CapabilitySelection & option_selection) noexcept
{
    try {
        InstanceExtensionManifest instance_manifest;
        DeviceExtensionManifest device_manifest;
        bs::PhysicalDeviceSelectInfo select_info = physical_device_select_info;
        for (std::size_t index = 0; index < capabilities.size(); ++index) {
            const CapabilityOption & option = (*capabilities[index])[option_selection[index]];
            option.register_requirements(instance_manifest, device_manifest);
        }
        select_info.setRequiredDeviceExtensionManifest(device_manifest);
        vk::ApplicationInfo app_info;
        bs::InstanceCreateInfo instance_info;
        instance_info.setApplicationInfo(app_info)
            .setRequiredInstanceExtensionManifest(instance_manifest);
        InstanceContext instance_context;
        if (auto failure = instance_context.create(instance_info)) {
            return std::unexpected(Error {failure.code(), failure.message()});
        }
        auto expected_physical_device = bs::select_physical_device(instance_context.getInstance(), select_info);
        if (not expected_physical_device) {
            return std::unexpected(expected_physical_device.error());
        }
        const vk::PhysicalDeviceProperties properties = expected_physical_device->getProperties();
        return ProbeResult {properties, capabilities, option_selection};
    } catch (const vk::SystemError & error) {
        return std::unexpected(Error {error.code(), error.what()});
    } catch (const std::exception & error) {
        return std::unexpected(Error {errc::no_suitable_instance, error.what()});
    }
}

} // namespace
