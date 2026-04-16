#include "Vulkan/configs/ext_functions.h"
#include "Vulkan/configs/requirements.h"
#include "Vulkan/configs/ext/debug_utils.h"

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
