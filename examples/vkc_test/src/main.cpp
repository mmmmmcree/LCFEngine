#include "vk_core/bootstrap/create_infos.h"
#include "vk_core/bootstrap/create_instance.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/surface/entry.h"
#include "log.h"

using namespace lcf;

int main()
{
    log::init();
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;

    vkc::dbg::register_debug_utils(inst_ext_manifest);
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
    } else {
        lcf_log_info("Instance created successfully");
    }
    auto _instance = std::move(expected_instance.value());
    auto instance = _instance.get();
    auto instance_ext_leases = inst_ext_manifest.enableExtensions(instance);
    lcf_log_info("leases count: {}", instance_ext_leases.size());

    auto physical_devices = instance.enumeratePhysicalDevices();
    for (const auto & physical_device : physical_devices) {
        lcf_log_info_if(device_ext_manifest.isRequiredFeaturesSupported(physical_device), "xxx");
    }

    // device_ext_manifest.    

    return 0;
}