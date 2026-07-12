#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/info_structs.h"
#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "log.h"

using namespace lcf;
namespace stdv = std::views;

int main()
{
    log::init();
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;

    vkc::entry::register_context(inst_ext_manifest, device_ext_manifest);

    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);
    
    vkc::InstanceContextCreateInfo instance_info; // same as bs::InstanceCreateInfo
    instance_info.setApplicationInfo(app_info)
        .setRequiredInstanceExtensionManifest(inst_ext_manifest);

    vkc::bs::PhysicalDeviceSelectInfo physical_device_select_info;
    physical_device_select_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPreferredType(vk::PhysicalDeviceType::eDiscreteGpu);
    vkc::DeviceContextCreateInfo device_context_info;
    device_context_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPhysicalDeviceSelectInfo(physical_device_select_info);
    
    vkc::InstanceContext instance_context;
    if (auto ec = instance_context.create(instance_info)) {
        lcf_log_error("Failed to create instance_context: {}", ec.message());
        return 1;
    }
    lcf_log_info("InstanceContext created successfully.");

    vkc::RenderDeviceContext render_device_context;
    if (auto ec = render_device_context.create(instance_context.getInstance(), device_context_info)) {
        lcf_log_error("Failed to create render_device_context: {}", ec.message());
        return 1;
    }
    lcf_log_info("graphics queue context address: {},\ncompute queue context address: {},\ntransfer queue context address: {}",
        static_cast<const void *>(&render_device_context.getGraphicsQueueContext()),
        static_cast<const void *>(&render_device_context.getComputeQueueContext()),
        static_cast<const void *>(&render_device_context.getTransferQueueContext()));

    return 0;
}
