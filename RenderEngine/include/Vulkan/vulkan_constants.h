#pragma once

#include "vulkan_enums.h"
#include "Vulkan/ds/VulkanDescriptorSetBinding.h"

namespace lcf::render::vkconstants {
    static constexpr vk::DescriptorSetLayoutBinding per_view_bindings[]
    {
        {
            std::to_underlying(PerViewBindingPoints::eCamera),
            // vk::DescriptorType::eUniformBufferDynamic,
            vk::DescriptorType::eUniformBuffer,
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


namespace ds {
    static constexpr uint32_t k_max_sets_per_pool = 64;
    static constexpr uint32_t k_max_variable_descriptor_count = 65536;
    static constexpr uint32_t k_initial_variable_descriptor_count = 256;
    static constexpr auto k_bindless_base_flags = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
            vk::DescriptorBindingFlagBits::ePartiallyBound |
            vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;

    static constexpr VulkanDescriptorSetBinding  k_per_view_bindings[]
    {
        {
            std::to_underlying(PerViewBindingPoints::eCamera),
            // vk::DescriptorType::eUniformBufferDynamic,
            vk::DescriptorType::eUniformBuffer,
            1u,
            vk::ShaderStageFlagBits::eVertex
        },
    };

    static constexpr VulkanDescriptorSetBinding k_bindless_buffer_bindings[]
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

    static constexpr VulkanDescriptorSetBinding k_bindless_texture_bindings[]
    {
        {
            vkenums::decode::get_binding_point(vkenums::DescriptorBindingPoint::eSamplers),
            vk::DescriptorType::eSampler,
            32u,
            vk::ShaderStageFlagBits::eFragment,
            k_bindless_base_flags
        },
        {
            vkenums::decode::get_binding_point(vkenums::DescriptorBindingPoint::eTextureCubes),
            vk::DescriptorType::eSampledImage,
            64u,
            vk::ShaderStageFlagBits::eFragment,
            k_bindless_base_flags
        },
        {
            vkenums::decode::get_binding_point(vkenums::DescriptorBindingPoint::eTexture2Ds),
            vk::DescriptorType::eSampledImage,
            k_max_variable_descriptor_count,
            vk::ShaderStageFlagBits::eFragment,
            k_bindless_base_flags | vk::DescriptorBindingFlagBits::eVariableDescriptorCount
        },
    };
}
}