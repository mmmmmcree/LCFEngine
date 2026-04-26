#include "Vulkan/configs/ext_functions.h"
#include "Vulkan/configs/requirements.h"
#include "Vulkan/configs/ext/debug_utils.h"
#include "Vulkan/configs/ext/device_generated_commands.h"

void lcf::render::vkext::load_instance_extensions(vk::Instance instance)
{
    auto & exts = vkreq::get_instance_extensions();
    if (exts.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        enable_debug_utils(instance);
    }
    // Future: if (exts.contains(VK_EXT_xxx)) { enable_xxx(instance); }
}

void lcf::render::vkext::release_instance_extensions_resources() noexcept
{
    release_debug_utils_resources();
}

void lcf::render::vkext::load_device_extensions(vk::Device device)
{
    auto & exts = vkreq::get_device_extensions();
    if (exts.contains(VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME)) {
        enable_device_generated_commands(device);
    }
    // Future: if (exts.contains(VK_EXT_xxx)) { enable_xxx(device); }
}

void lcf::render::vkext::release_device_extensions_resources() noexcept
{
    release_device_generated_commands_resources();
}
