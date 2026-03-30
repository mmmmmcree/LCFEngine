#pragma once

#include "vulkan_enums.h"

namespace lcf::render::vkconstants {
    static constexpr vk::DescriptorSetLayoutBinding per_view_bindings[]
    {
        {
            std::to_underlying(PerViewBindingPoints::eCamera),
            vk::DescriptorType::eUniformBufferDynamic,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
    };

    static constexpr vk::DescriptorSetLayoutBinding per_renderable_bindings[]
    {
        {
            vkenums::decode::get_binding_point(vkenums::DescriptorBindingPoint::eVertexBufferAddresses),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
        {
            vkenums::decode::get_binding_point(vkenums::DescriptorBindingPoint::eIndexBufferAddresses),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
        {
            vkenums::decode::get_binding_point(vkenums::DescriptorBindingPoint::eTransforms),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
        {
            vkenums::decode::get_binding_point(vkenums::DescriptorBindingPoint::eMaterialRecords),
            vk::DescriptorType::eStorageBuffer,
            1u,
            vk::ShaderStageFlagBits::eFragment
        },
    };
namespace bindless {
    static constexpr uint32_t k_max_variable_descriptor_count = 65536;
    static constexpr uint32_t k_initial_variable_descriptor_count = 256;
}
}