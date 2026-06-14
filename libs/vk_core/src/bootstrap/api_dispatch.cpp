#include "vk_core/bootstrap/api_dispatch.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace lcf::vkc::bs {

void initialize_loader() noexcept
{
    static bool s_loaded = [] {
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        return true;
    }();
    (void)s_loaded;
}

void initialize_instance(vk::Instance instance) noexcept
{
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
}

void initialize_device(vk::Device device) noexcept
{
#ifdef LCF_VKC_USE_DEVICE_DISPATCH
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
#else
    (void)device;
#endif
}

} // namespace lcf::vkc::bs
