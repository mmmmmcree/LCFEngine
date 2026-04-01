#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::render {

    struct VulkanDescriptorSetBinding : vk::DescriptorSetLayoutBinding
    {
        using Base = vk::DescriptorSetLayoutBinding;
        using Base::Base;
        vk::DescriptorBindingFlags flags = {};  // merged binding flags (set by caller, e.g. ShaderProgram link step)

        bool hasFlag(vk::DescriptorBindingFlagBits flag) const noexcept
        {
            return static_cast<bool>(flags & flag);
        }
    };

}
