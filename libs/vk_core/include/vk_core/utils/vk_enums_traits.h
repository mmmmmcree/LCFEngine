#pragma once

#include <cstdint>
#include <utility>
#include <vulkan/vulkan_enums.hpp>
#include "enums/enum_traits.h"

template <>
struct lcf::enum_traits<vk::ShaderStageFlagBits>
{
private:
    using Stage = vk::ShaderStageFlagBits;
    using StageFlags = vk::ShaderStageFlags;
public:
    static constexpr StageFlags valid_next_stages_of(Stage stage) noexcept
    {
        switch (stage) {
            case Stage::eVertex:
                return Stage::eTessellationControl | Stage::eGeometry | Stage::eFragment;
            case Stage::eTessellationControl:
                return Stage::eTessellationEvaluation;
            case Stage::eTessellationEvaluation:
                return Stage::eGeometry | Stage::eFragment;
            case Stage::eGeometry:
            case Stage::eMeshEXT:
                return Stage::eFragment;
            case Stage::eTaskEXT:
                return Stage::eMeshEXT;
            default:
                return {};
        }
    }
    static constexpr bool is_valid_next_stage_of(Stage stage, StageFlags next_stages) noexcept
    {
        return (valid_next_stages_of(stage) & next_stages) == next_stages;
    }
    static constexpr uint32_t order_of(Stage stage) noexcept
    {
        switch (stage) {
            case Stage::eTaskEXT: return 0u;
            case Stage::eVertex: return 1u;
            case Stage::eTessellationControl: return 2u;
            case Stage::eTessellationEvaluation: return 3u;
            case Stage::eGeometry: return 4u;
            case Stage::eMeshEXT: return 5u;
            case Stage::eFragment: return 6u;
            case Stage::eCompute: return 7u;
            default: return 8u;
        }
    }
    static constexpr bool precedes_in_pipeline(Stage lhs, Stage rhs) noexcept
    {
        const auto lhs_order = order_of(lhs);
        const auto rhs_order = order_of(rhs);
        return lhs_order == rhs_order ? std::to_underlying(lhs) < std::to_underlying(rhs) : lhs_order < rhs_order;
    }
    struct pipeline_order_less_t
    {
        constexpr bool operator()(Stage lhs, Stage rhs) const noexcept
        {
            return precedes_in_pipeline(lhs, rhs);
        }
    };
};

template <>
struct lcf::enum_traits<vk::DescriptorType>
{
private:
    using DescriptorType = vk::DescriptorType;
public:
    static constexpr bool is_buffer_descriptor(DescriptorType type) noexcept
    {
        return type == DescriptorType::eUniformBuffer or
            type == DescriptorType::eStorageBuffer or
            type == DescriptorType::eUniformBufferDynamic or
            type == DescriptorType::eStorageBufferDynamic;
    }
    static constexpr bool is_image_descriptor(DescriptorType type) noexcept
    {
        return type == DescriptorType::eCombinedImageSampler or
            type == DescriptorType::eSampledImage or
            type == DescriptorType::eStorageImage or
            type == DescriptorType::eInputAttachment;
    }
    static constexpr bool is_sampler_descriptor(DescriptorType type) noexcept
    {
        return type == DescriptorType::eSampler or type == DescriptorType::eCombinedImageSampler;
    }
};

template <>
struct lcf::enum_traits<vk::Format>
{
    static constexpr bool is_depth_format(vk::Format format) noexcept
    {
        return format >= vk::Format::eD16Unorm and format <= vk::Format::eD32SfloatS8Uint and format != vk::Format::eS8Uint;
    }
    static constexpr bool is_stencil_format(vk::Format format) noexcept
    {
        return format >= vk::Format::eS8Uint and format <= vk::Format::eD32SfloatS8Uint;
    }
    static constexpr bool is_depth_stencil_format(vk::Format format) noexcept
    {
        return format >= vk::Format::eD16UnormS8Uint and format <= vk::Format::eD32SfloatS8Uint;
    }
};

template <>
struct lcf::enum_traits<vk::AttachmentLoadOp>
{
    static constexpr bool is_discarding(vk::AttachmentLoadOp load_op) noexcept
    {
        return load_op == vk::AttachmentLoadOp::eClear or load_op == vk::AttachmentLoadOp::eDontCare;
    }
    static constexpr bool discards_on(
        vk::Format format,
        vk::AttachmentLoadOp load_op,
        vk::AttachmentLoadOp stencil_load_op) noexcept
    {
        bool load_discards = is_discarding(load_op);
        if (not lcf::enum_traits<vk::Format>::is_stencil_format(format)) { return load_discards; }
        bool stencil_load_discards = is_discarding(stencil_load_op);
        if (not lcf::enum_traits<vk::Format>::is_depth_format(format)) { return stencil_load_discards; }
        return load_discards and stencil_load_discards;
    }
};

template <>
struct lcf::enum_traits<vk::AttachmentStoreOp>
{
    static constexpr bool is_discarding(vk::AttachmentStoreOp store_op) noexcept
    {
        return store_op == vk::AttachmentStoreOp::eDontCare;
    }
    static constexpr bool discards_on(
        vk::Format format,
        vk::AttachmentStoreOp store_op,
        vk::AttachmentStoreOp stencil_store_op) noexcept
    {
        bool store_discards = is_discarding(store_op);
        if (not lcf::enum_traits<vk::Format>::is_stencil_format(format)) { return store_discards; }
        bool stencil_store_discards = is_discarding(stencil_store_op);
        if (not lcf::enum_traits<vk::Format>::is_depth_format(format)) { return stencil_store_discards; }
        return store_discards and stencil_store_discards;
    }
};

template <>
struct lcf::enum_traits<vk::ImageLayout>
{
    static constexpr vk::ImageLayout to_unified(vk::ImageLayout specific_layout) noexcept
    {
        switch (specific_layout) {
            case vk::ImageLayout::eUndefined:
            case vk::ImageLayout::ePresentSrcKHR:
                return specific_layout;
            default:
                return vk::ImageLayout::eGeneral;
        }
    }
};
