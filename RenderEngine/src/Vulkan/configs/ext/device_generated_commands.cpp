#include "Vulkan/configs/ext/device_generated_commands.h"
#include "log.h"

// ============================================================================
//  Static dispatch: extension functions must be loaded manually via
//  vkGetDeviceProcAddr. In the dynamic-dispatch build, vulkan-hpp already
//  resolves every entry point inside VULKAN_HPP_DEFAULT_DISPATCHER.init(device)
//  so this whole translation unit becomes essentially empty.
// ============================================================================

#if !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC

static PFN_vkGetGeneratedCommandsMemoryRequirementsEXT s_pfnGetGeneratedCommandsMemoryRequirementsEXT = nullptr;
static PFN_vkCmdPreprocessGeneratedCommandsEXT s_pfnCmdPreprocessGeneratedCommandsEXT = nullptr;
static PFN_vkCmdExecuteGeneratedCommandsEXT s_pfnCmdExecuteGeneratedCommandsEXT = nullptr;
static PFN_vkCreateIndirectCommandsLayoutEXT s_pfnCreateIndirectCommandsLayoutEXT = nullptr;
static PFN_vkDestroyIndirectCommandsLayoutEXT s_pfnDestroyIndirectCommandsLayoutEXT = nullptr;
static PFN_vkCreateIndirectExecutionSetEXT s_pfnCreateIndirectExecutionSetEXT = nullptr;
static PFN_vkDestroyIndirectExecutionSetEXT s_pfnDestroyIndirectExecutionSetEXT = nullptr;
static PFN_vkUpdateIndirectExecutionSetPipelineEXT s_pfnUpdateIndirectExecutionSetPipelineEXT = nullptr;
static PFN_vkUpdateIndirectExecutionSetShaderEXT s_pfnUpdateIndirectExecutionSetShaderEXT = nullptr;

// ---------------------------------------------------------------------------
//  Forwarding symbols so that both the raw C API and vulkan-hpp wrappers work
// ---------------------------------------------------------------------------

VKAPI_ATTR void VKAPI_CALL vkGetGeneratedCommandsMemoryRequirementsEXT(
    VkDevice device,
    const VkGeneratedCommandsMemoryRequirementsInfoEXT * pInfo,
    VkMemoryRequirements2 * pMemoryRequirements)
{
    s_pfnGetGeneratedCommandsMemoryRequirementsEXT(device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL vkCmdPreprocessGeneratedCommandsEXT(
    VkCommandBuffer commandBuffer,
    const VkGeneratedCommandsInfoEXT * pGeneratedCommandsInfo,
    VkCommandBuffer stateCommandBuffer)
{
    s_pfnCmdPreprocessGeneratedCommandsEXT(commandBuffer, pGeneratedCommandsInfo, stateCommandBuffer);
}

VKAPI_ATTR void VKAPI_CALL vkCmdExecuteGeneratedCommandsEXT(
    VkCommandBuffer commandBuffer,
    VkBool32 isPreprocessed,
    const VkGeneratedCommandsInfoEXT * pGeneratedCommandsInfo)
{
    s_pfnCmdExecuteGeneratedCommandsEXT(commandBuffer, isPreprocessed, pGeneratedCommandsInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectCommandsLayoutEXT(
    VkDevice device,
    const VkIndirectCommandsLayoutCreateInfoEXT * pCreateInfo,
    const VkAllocationCallbacks * pAllocator,
    VkIndirectCommandsLayoutEXT * pIndirectCommandsLayout)
{
    return s_pfnCreateIndirectCommandsLayoutEXT(device, pCreateInfo, pAllocator, pIndirectCommandsLayout);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectCommandsLayoutEXT(
    VkDevice device,
    VkIndirectCommandsLayoutEXT indirectCommandsLayout,
    const VkAllocationCallbacks * pAllocator)
{
    s_pfnDestroyIndirectCommandsLayoutEXT(device, indirectCommandsLayout, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectExecutionSetEXT(
    VkDevice device,
    const VkIndirectExecutionSetCreateInfoEXT * pCreateInfo,
    const VkAllocationCallbacks * pAllocator,
    VkIndirectExecutionSetEXT * pIndirectExecutionSet)
{
    return s_pfnCreateIndirectExecutionSetEXT(device, pCreateInfo, pAllocator, pIndirectExecutionSet);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectExecutionSetEXT(
    VkDevice device,
    VkIndirectExecutionSetEXT indirectExecutionSet,
    const VkAllocationCallbacks * pAllocator)
{
    s_pfnDestroyIndirectExecutionSetEXT(device, indirectExecutionSet, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL vkUpdateIndirectExecutionSetPipelineEXT(
    VkDevice device,
    VkIndirectExecutionSetEXT indirectExecutionSet,
    uint32_t executionSetWriteCount,
    const VkWriteIndirectExecutionSetPipelineEXT * pExecutionSetWrites)
{
    s_pfnUpdateIndirectExecutionSetPipelineEXT(device, indirectExecutionSet,
        executionSetWriteCount, pExecutionSetWrites);
}

VKAPI_ATTR void VKAPI_CALL vkUpdateIndirectExecutionSetShaderEXT(
    VkDevice device,
    VkIndirectExecutionSetEXT indirectExecutionSet,
    uint32_t executionSetWriteCount,
    const VkWriteIndirectExecutionSetShaderEXT * pExecutionSetWrites)
{
    s_pfnUpdateIndirectExecutionSetShaderEXT(device, indirectExecutionSet,
        executionSetWriteCount, pExecutionSetWrites);
}

#endif // !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC

// ============================================================================
//  Public enable / release
// ============================================================================

void lcf::render::vkext::enable_device_generated_commands(vk::Device device)
{
#if !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    s_pfnGetGeneratedCommandsMemoryRequirementsEXT = reinterpret_cast<PFN_vkGetGeneratedCommandsMemoryRequirementsEXT>(
        device.getProcAddr("vkGetGeneratedCommandsMemoryRequirementsEXT"));
    s_pfnCmdPreprocessGeneratedCommandsEXT = reinterpret_cast<PFN_vkCmdPreprocessGeneratedCommandsEXT>(
        device.getProcAddr("vkCmdPreprocessGeneratedCommandsEXT"));
    s_pfnCmdExecuteGeneratedCommandsEXT = reinterpret_cast<PFN_vkCmdExecuteGeneratedCommandsEXT>(
        device.getProcAddr("vkCmdExecuteGeneratedCommandsEXT"));
    s_pfnCreateIndirectCommandsLayoutEXT = reinterpret_cast<PFN_vkCreateIndirectCommandsLayoutEXT>(
        device.getProcAddr("vkCreateIndirectCommandsLayoutEXT"));
    s_pfnDestroyIndirectCommandsLayoutEXT = reinterpret_cast<PFN_vkDestroyIndirectCommandsLayoutEXT>(
        device.getProcAddr("vkDestroyIndirectCommandsLayoutEXT"));
    s_pfnCreateIndirectExecutionSetEXT = reinterpret_cast<PFN_vkCreateIndirectExecutionSetEXT>(
        device.getProcAddr("vkCreateIndirectExecutionSetEXT"));
    s_pfnDestroyIndirectExecutionSetEXT = reinterpret_cast<PFN_vkDestroyIndirectExecutionSetEXT>(
        device.getProcAddr("vkDestroyIndirectExecutionSetEXT"));
    s_pfnUpdateIndirectExecutionSetPipelineEXT = reinterpret_cast<PFN_vkUpdateIndirectExecutionSetPipelineEXT>(
        device.getProcAddr("vkUpdateIndirectExecutionSetPipelineEXT"));
    s_pfnUpdateIndirectExecutionSetShaderEXT = reinterpret_cast<PFN_vkUpdateIndirectExecutionSetShaderEXT>(
        device.getProcAddr("vkUpdateIndirectExecutionSetShaderEXT"));

    if (not s_pfnCmdExecuteGeneratedCommandsEXT or
        not s_pfnCreateIndirectCommandsLayoutEXT or
        not s_pfnCreateIndirectExecutionSetEXT) {
        lcf_log_error("Failed to load VK_EXT_device_generated_commands entry points. "
            "Extension not supported by the current device?");
    }
#else
    (void)device; // All entry points are resolved by vulkan-hpp's dynamic dispatcher.
#endif
}

void lcf::render::vkext::release_device_generated_commands_resources() noexcept
{
}
