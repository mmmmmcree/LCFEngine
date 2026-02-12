#pragma once

#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanTimelineSemaphore;
    LCF_DECLARE_POINTER_DEFS(VulkanTimelineSemaphore, STDPointerDefs);

    class VulkanCommandBufferObject;

    class VulkanImage;
    LCF_DECLARE_POINTER_DEFS(VulkanImage, STDPointerDefs);

    class VulkanBuffer;
    LCF_DECLARE_POINTER_DEFS(VulkanBuffer, STDPointerDefs);

    class VulkanBufferObject;
    LCF_DECLARE_POINTER_DEFS(VulkanBufferObject, STDPointerDefs);

    class VulkanBufferObjectGroup;
}