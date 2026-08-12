#include "vk_core/bootstrap/create_device.h"
#include "vk_core/bootstrap/info_structs.h"
#include "vk_core/bootstrap/api_dispatch.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/error.h"

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;
using namespace lcf::vkc::bs;

std::expected<vk::UniqueDevice, Error> create_device_maythrow(vk::PhysicalDevice physical_device, const DeviceCreateInfo & create_info);

} // namespace

namespace lcf::vkc::bs {

std::expected<vk::UniqueDevice, Error> create_device(vk::PhysicalDevice physical_device, const DeviceCreateInfo & create_info) noexcept
{
    std::expected<vk::UniqueDevice, Error> expected_device;
    try {
        expected_device = create_device_maythrow(physical_device, create_info);
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    if (not expected_device) { return expected_device; }
    initialize_device(expected_device->get());
    return expected_device;
}

} // namespace lcf::vkc::bs

namespace {

std::expected<vk::UniqueDevice, Error> create_device_maythrow(vk::PhysicalDevice physical_device, const DeviceCreateInfo & create_info)
{
    auto device_extension_props = physical_device.enumerateDeviceExtensionProperties() |
        stdv::filter([&](const vk::ExtensionProperties & ext_props) { return create_info.isExtensionRequired(ext_props.extensionName.data()); }) |
        stdr::to<std::vector>();
    if (device_extension_props.size() != create_info.getRequiredDeviceExtensionCount()) {
        return std::unexpected(Error {errc::missing_required_device_extension, create_info.getUnsupportedExtensionsMessage(physical_device)});
    }
    if (not create_info.isRequiredFeaturesSupported(physical_device)) {
        return std::unexpected(Error {errc::missing_required_device_feature, create_info.getUnsupportedFeaturesMessage(physical_device)});
    }
    auto device_extension_names_cstr = device_extension_props |
        stdv::transform([](const vk::ExtensionProperties & extension) { return extension.extensionName.data(); }) |
        stdr::to<std::vector>();
    const auto & queue_family_requests = create_info.getQueueFamilyRequests();
    if (queue_family_requests.empty()) { return std::unexpected(make_error_code(errc::no_device_queue_requested)); }
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(queue_family_requests.size());
    for (const auto &[queue_family_index, priorities] : queue_family_requests) {
        vk::DeviceQueueCreateInfo queue_create_info;
        queue_create_info.setQueueFamilyIndex(queue_family_index)
            .setQueuePriorities(priorities);
        queue_create_infos.emplace_back(queue_create_info);
    }
    vk::DeviceCreateInfo device_create_info;
    device_create_info.setQueueCreateInfos(queue_create_infos)
        .setPEnabledExtensionNames(device_extension_names_cstr)
        .setPNext(create_info.getRequiredFeatures());
    return physical_device.createDeviceUnique(device_create_info);
}

} // namespace