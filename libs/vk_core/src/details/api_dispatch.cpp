#include "vk_core/details/api_dispatch.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace lcf::vkc::details {

void initialize_loader()
{
    static bool s_loaded = [] {
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        return true;
    }();
    (void)s_loaded;
}

void initialize_instance(vk::Instance instance)
{
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
}

void initialize_device(vk::Device device)
{
    // 注意：调用 init(device) 会用 vkGetDeviceProcAddr 把 device-level entry points
    // 替换为 driver 直连版本（绕过 instance layer chain），导致 Nsight Systems 等
    // 通过 implicit layer 拦截 vkCmd*/vkQueueSubmit 的工具失效（只能抓到 instance-level
    // 的 init API）。
    // 如需极致性能调度（生产构建），定义 LCF_USE_DEVICE_DISPATCH 启用 device 级直连。
#ifdef LCF_VKC_USE_DEVICE_DISPATCH
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
#else
    (void)device;
#endif
}

} // namespace lcf::vkc::details
