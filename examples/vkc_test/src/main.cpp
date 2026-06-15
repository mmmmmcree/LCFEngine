#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/surface/entry.h"
#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/bootstrap/create_instance.h"
#include "vk_core/bootstrap/select_physical_device.h"
#include "log.h"

using namespace lcf;

int main()
{
    log::init();
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;

    vkc::dbg::DebugLogCallbacks debug_callbacks;
    debug_callbacks.setWarningSink([](std::string_view message) { lcf_log_warn(message); })
        .setErrorSink([](std::string_view message) { lcf_log_error(message); });
    vkc::dbg::register_debug_utils(inst_ext_manifest, vkc::dbg::SeverityFlags::eError | vkc::dbg::SeverityFlags::eWarning, debug_callbacks);
    vkc::surf::register_surface(inst_ext_manifest);
    vkc::surf::register_swapchain(device_ext_manifest);

    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);
    
    vkc::bs::InstanceCreateInfo instance_info;
    instance_info.setApplicationInfo(app_info)
        .addRequiredInstanceLayer("VK_LAYER_KHRONOS_validation")
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

    return 0;
}