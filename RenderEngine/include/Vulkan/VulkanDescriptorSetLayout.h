#pragma once

#include "common/enum_types.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render::vkconstants {
    static constexpr vk::DescriptorSetLayoutBinding per_view_bindings[] = {
        {
            static_cast<uint32_t>(PerViewBindingPoints::eCamera),
            vk::DescriptorType::eUniformBufferDynamic,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
    };

    static constexpr vk::DescriptorSetLayoutBinding per_renderable_bindings[] = {
        {
            static_cast<uint32_t>(PerRenderableBindingPoints::eTransform),
            vk::DescriptorType::eUniformBufferDynamic,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
    };
}