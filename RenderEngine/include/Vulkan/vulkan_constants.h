#pragma once

#include "common/render_enums.h"
#include "enum_cast.h"
#include <vulkan/vulkan.hpp>

namespace lcf::render::vkconstants {
    static constexpr vk::DescriptorSetLayoutBinding per_view_bindings[] = {
        {
            to_integral(PerViewBindingPoints::eCamera),
            vk::DescriptorType::eUniformBufferDynamic,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
    };

    static constexpr vk::DescriptorSetLayoutBinding per_renderable_bindings[] = {
        {
            to_integral(PerRenderableBindingPoints::eVertexBuffer),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
        {
            to_integral(PerRenderableBindingPoints::eIndexBuffer),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
        {
            to_integral(PerRenderableBindingPoints::eTransform),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
        {
            to_integral(PerRenderableBindingPoints::eMaterialIndexing),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        }
    };
}