#include "vk_core/details/api_dispatch.h"

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

namespace lcf::vkc::details {

void initialize_loader()
{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    static bool s_loaded = [] {
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        return true;
    }();
    (void)s_loaded;
#endif
}

void initialize_instance(vk::Instance instance)
{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
#else
    (void)instance;
#endif
}

void initialize_device(vk::Device device)
{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    // 注意：调用 init(device) 会用 vkGetDeviceProcAddr 把 device-level entry points
    // 替换为 driver 直连版本（绕过 instance layer chain），导致 Nsight Systems 等
    // 通过 implicit layer 拦截 vkCmd*/vkQueueSubmit 的工具失效（只能抓到 instance-level
    // 的 init API）。代价：保留 instance proc addr 加载的版本，性能多一层 trampoline，
    // Release 下额外 1-3% 调度开销，但换来 nsys / RenderDoc 完全可观测，对 benchmark
    // 双源认证至关重要（chap05 §5.5.6）。
    //
    // 如需极致性能调度（生产构建），定义 LCF_USE_DEVICE_DISPATCH 启用 device 级直连。
  #ifdef LCF_USE_DEVICE_DISPATCH
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
  #else
    (void)device;
  #endif
#else
    (void)device;
#endif
}

} // namespace lcf::vkc::details

