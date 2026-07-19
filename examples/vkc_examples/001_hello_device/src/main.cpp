
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/bootstrap/info_structs.h"
#include "vk_core/bootstrap/create_instance.h"
#include "vk_core/bootstrap/select_physical_device.h"
#include "vk_core/bootstrap/create_device.h"
#include "log.h"

using namespace lcf;
namespace stdv = std::views;

int main()
{
    log::init();
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;

    vkc::dbg::DebugLogCallbacks debug_callbacks;
    debug_callbacks.setWarningSink([](std::string_view message) { lcf_log_warn(message); })
        .setErrorSink([](std::string_view message) { lcf_log_error(message); });
    vkc::entry::register_debug_utils(inst_ext_manifest, vkc::dbg::SeverityFlags::eError | vkc::dbg::SeverityFlags::eWarning, debug_callbacks);

    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);
    
    vkc::bs::InstanceCreateInfo instance_info;
    instance_info.setApplicationInfo(app_info)
        .addRequiredInstanceLayer("VK_LAYER_KHRONOS_validation")
        // .addRequiredInstanceLayer("SomeStupidLayer")
        .setRequiredInstanceExtensionManifest(inst_ext_manifest);
    auto expected_instance = vkc::bs::create_instance(instance_info);
    if (not expected_instance.has_value()) {
        lcf_log_error(expected_instance.error().message());
        return 1;
    }
    lcf_log_info("Instance created successfully");
    auto _instance = std::move(expected_instance.value());
    auto instance = _instance.get();
    auto instance_ext_leases = inst_ext_manifest.enableExtensions(instance);
    lcf_log_info("leases count: {}", instance_ext_leases.size());
    vkc::bs::PhysicalDeviceSelectInfo physical_device_select_info;
    physical_device_select_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPreferredType(vk::PhysicalDeviceType::eDiscreteGpu);
    auto expected_physical_device = vkc::bs::select_physical_device(instance, physical_device_select_info);
    if (not expected_physical_device.has_value()) {
        lcf_log_error(expected_physical_device.error().message());
        return 1;
    }
    auto physical_device = std::move(expected_physical_device.value());
    lcf_log_info("Physical device selected successfully, device name: {}", std::string(physical_device.getProperties().deviceName.data()));

    uint32_t queue_family_index = 0;
    for (const auto & [index, props] : physical_device.getQueueFamilyProperties() | stdv::enumerate) {
        if (props.queueFlags & vk::QueueFlagBits::eGraphics) {
            queue_family_index = index;
            break;
        }
    }
    vkc::bs::DeviceCreateInfo device_info;
    device_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .addQueueFamilyRequest({queue_family_index, 1.0f});
    auto expected_device = vkc::bs::create_device(physical_device, device_info);
    if (not expected_device.has_value()) {
        lcf_log_error(expected_device.error().message());
        return 1;
    }
    auto _device = std::move(expected_device.value());
    auto device = _device.get();
    lcf_log_info("Device created successfully");

    lcf_log_info("Triggering a deliberate validation error to test the debug messenger...");
    vk::BufferCreateInfo bad_buffer_info;
    bad_buffer_info.setSize(0)
        .setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);
    auto bad_buffer = device.createBufferUnique(bad_buffer_info);

    return 0;
}