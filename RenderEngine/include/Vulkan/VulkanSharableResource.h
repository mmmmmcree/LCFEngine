#pragma once

#include "vulkan_memory_resources.h"
#include "VulkanDescriptorSet.h"
#include <variant>

namespace lcf::render {
    using VulkanSharableResource = std::variant<
        typename VulkanImage::SharedPointer,
        typename VulkanBuffer::SharedPointer,
        typename VulkanDescriptorSet::SharedPointer
    >;
}