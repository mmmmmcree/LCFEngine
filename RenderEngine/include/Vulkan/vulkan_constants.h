#pragma once

#include "common/render_enums.h"
#include "enum_cast.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render::vkconstants {
    static constexpr vk::DescriptorSetLayoutBinding per_view_bindings[] = {
        {
            std::to_underlying(PerViewBindingPoints::eCamera),
            vk::DescriptorType::eUniformBufferDynamic,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
    };

    static constexpr vk::DescriptorSetLayoutBinding per_renderable_bindings[] = {
        {
            std::to_underlying(PerRenderableBindingPoints::eVertexBuffer),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
        {
            std::to_underlying(PerRenderableBindingPoints::eIndexBuffer),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
        {
            std::to_underlying(PerRenderableBindingPoints::eTransform),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
    };
}