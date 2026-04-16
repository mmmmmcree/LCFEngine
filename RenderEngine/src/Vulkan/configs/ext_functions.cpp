#include "Vulkan/configs/ext_functions.h"

#if !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC

// ============================================================================
//  Static dispatch: extension functions must be loaded manually via getProcAddr
//  and forwarded through C trampoline functions.
// ============================================================================

#ifndef NDEBUG
static PFN_vkCreateDebugUtilsMessengerEXT  s_pfnCreateDebugUtilsMessengerEXT  = nullptr;
static PFN_vkDestroyDebugUtilsMessengerEXT s_pfnDestroyDebugUtilsMessengerEXT = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
    const VkAllocationCallbacks * pAllocator,
    VkDebugUtilsMessengerEXT * pMessenger)
{
    return s_pfnCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks * pAllocator)
{
    return s_pfnDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}
#endif // NDEBUG

void lcf::render::vkext::load_instance_extensions(vk::Instance instance)
{
#ifndef NDEBUG
    s_pfnCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
    s_pfnDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
#else
    (void)instance;
#endif
}

#else // VULKAN_HPP_DISPATCH_LOADER_DYNAMIC

// ============================================================================
//  Dynamic dispatch: the dispatcher resolves all extension functions
//  automatically during initInstance/initDevice. Nothing to do here.
// ============================================================================

void lcf::render::vkext::load_instance_extensions(vk::Instance)
{
}

#endif
