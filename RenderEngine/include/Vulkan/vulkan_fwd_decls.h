#pragma once

#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanTimelineSemaphore;
    LCF_DECLARE_POINTER_DEFS(VulkanTimelineSemaphore, STDPointerDefs);

    class VulkanCommandBufferObject;

    class VulkanImage;

    class VulkanBuffer;

    class VulkanBufferProxy;

    class VulkanBufferObject;
    LCF_DECLARE_POINTER_DEFS(VulkanBufferObject, STDPointerDefs);

    class VulkanBufferObjectGroup;

    class VulkanImageProxy;

    class VulkanImageObject;
    LCF_DECLARE_POINTER_DEFS(VulkanImageObject, STDPointerDefs);

    class VulkanSwapchain;
    LCF_DECLARE_POINTER_DEFS(VulkanSwapchain, STDPointerDefs);

    class VulkanFramebufferObject;

    class VulkanShader;
    LCF_DECLARE_POINTER_DEFS(VulkanShader, STDPointerDefs);

    class VulkanShaderProgram;
    LCF_DECLARE_POINTER_DEFS(VulkanShaderProgram, STDPointerDefs);

    class VulkanSampler;
    LCF_DECLARE_POINTER_DEFS(VulkanSampler, STDPointerDefs);

    class VulkanDescriptorSetBinding;
    class VulkanDescriptorSet2;
    class VulkanDescriptorSetLayout2;
    class VulkanBindlessDescriptorSet;
    class VulkanDescriptorSetManager;
}