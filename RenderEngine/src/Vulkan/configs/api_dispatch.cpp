#include "Vulkan/configs/api_dispatch.h"

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

void lcf::render::vkdispatch::initialize_loader()
{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    static bool s_loaded = [] {
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        return true;
    }();
    (void)s_loaded;
#endif
}

void lcf::render::vkdispatch::initialize_instance(vk::Instance instance)
{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
#else
    (void)instance;
#endif
}

void lcf::render::vkdispatch::initialize_device(vk::Device device)
{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
#else
    (void)device;
#endif
}