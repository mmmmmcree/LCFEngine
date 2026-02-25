#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_fwd_decls.h"
#include <functional>
#include "common/render_enums.h"

namespace lcf::render::vkutils {
    using TransitionDependency = std::tuple<vk::PipelineStageFlags2, vk::AccessFlags2, vk::PipelineStageFlags2, vk::AccessFlags2>;

    template <vk::QueueFlagBits queue_type>
    constexpr  TransitionDependency get_image_layout_transition_dependency(vk::ImageLayout current_layout, vk::ImageLayout new_layout);

    template <>
    constexpr TransitionDependency get_image_layout_transition_dependency<vk::QueueFlagBits::eTransfer>(
        vk::ImageLayout current_layout, vk::ImageLayout new_layout)
    {
        vk::PipelineStageFlags2 src_stage, dst_stage;
        vk::AccessFlags2 src_access, dst_access;
        
        switch (current_layout) {
            case vk::ImageLayout::eTransferSrcOptimal: {
                src_stage = vk::PipelineStageFlagBits2::eTransfer;
                src_access = vk::AccessFlagBits2::eTransferRead;
            } break;
            case vk::ImageLayout::eTransferDstOptimal: {
                src_stage = vk::PipelineStageFlagBits2::eTransfer;
                src_access = vk::AccessFlagBits2::eTransferWrite;
            } break;
            case vk::ImageLayout::eGeneral: {
                src_stage = vk::PipelineStageFlagBits2::eTransfer;
                src_access = vk::AccessFlagBits2::eTransferRead | vk::AccessFlagBits2::eTransferWrite;
            } break;
            default: {
                src_stage = vk::PipelineStageFlagBits2::eTopOfPipe;
                src_access = vk::AccessFlagBits2::eNone;
            } break;
        }
        switch (new_layout) {
            case vk::ImageLayout::eTransferSrcOptimal: {
                dst_stage = vk::PipelineStageFlagBits2::eTransfer;
                dst_access = vk::AccessFlagBits2::eTransferRead;
            } break;
            case vk::ImageLayout::eTransferDstOptimal: {
                dst_stage = vk::PipelineStageFlagBits2::eTransfer;
                dst_access = vk::AccessFlagBits2::eTransferWrite;
            } break;
            case vk::ImageLayout::eGeneral: {
                dst_stage = vk::PipelineStageFlagBits2::eTransfer;
                dst_access = vk::AccessFlagBits2::eTransferRead | vk::AccessFlagBits2::eTransferWrite;
            } break;
            default: {
                dst_stage = vk::PipelineStageFlagBits2::eBottomOfPipe;
                dst_access = vk::AccessFlagBits2::eNone;
            } break;
        }
        return std::make_tuple(src_stage, src_access, dst_stage, dst_access);
    }

    template <>
    constexpr TransitionDependency get_image_layout_transition_dependency<vk::QueueFlagBits::eGraphics>(
        vk::ImageLayout current_layout, vk::ImageLayout new_layout)
    {
        vk::PipelineStageFlags2 src_stage, dst_stage;
        vk::AccessFlags2 src_access, dst_access;
        switch (current_layout) {
            case vk::ImageLayout::eUndefined: {
                src_stage = vk::PipelineStageFlagBits2::eAllGraphics;
                src_access = vk::AccessFlagBits2::eMemoryRead;
            } break;
            case vk::ImageLayout::eColorAttachmentOptimal: {
                src_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
                src_access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite;
            } break;
            case vk::ImageLayout::eShaderReadOnlyOptimal: {
                src_stage = vk::PipelineStageFlagBits2::eFragmentShader;
                src_access = vk::AccessFlagBits2::eShaderRead;
            } break;
            case vk::ImageLayout::eTransferSrcOptimal: {
                src_stage = vk::PipelineStageFlagBits2::eTransfer;
                src_access = vk::AccessFlagBits2::eTransferRead;
            } break;
            case vk::ImageLayout::eTransferDstOptimal: {
                src_stage = vk::PipelineStageFlagBits2::eTransfer;
                src_access = vk::AccessFlagBits2::eTransferWrite;
            } break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal: {
                src_stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
                src_access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
            } break;
            default: {
                src_stage = vk::PipelineStageFlagBits2::eTopOfPipe;
                src_access = vk::AccessFlagBits2::eNone;
            } break;
        }
        switch (new_layout) {
            case vk::ImageLayout::eColorAttachmentOptimal: {
                dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput | vk::PipelineStageFlagBits2::eFragmentShader;
                dst_access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite;
            } break;
            case vk::ImageLayout::eShaderReadOnlyOptimal: {
                dst_stage = vk::PipelineStageFlagBits2::eFragmentShader;
                dst_access = vk::AccessFlagBits2::eShaderRead;
            } break;
            case vk::ImageLayout::eTransferSrcOptimal: {
                dst_stage = vk::PipelineStageFlagBits2::eTransfer;
                dst_access = vk::AccessFlagBits2::eTransferRead;
            } break;
            case vk::ImageLayout::eTransferDstOptimal: {
                dst_stage = vk::PipelineStageFlagBits2::eTransfer;
                dst_access = vk::AccessFlagBits2::eTransferWrite;
            } break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal: {
                dst_stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
                dst_access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
            } break;
            default: {
                dst_stage = vk::PipelineStageFlagBits2::eTopOfPipe;
                dst_access = vk::AccessFlagBits2::eNone;
            } break;
        }
        return std::make_tuple(src_stage, src_access, dst_stage, dst_access);
    }

    constexpr TransitionDependency get_image_layout_transition_dependency(vk::ImageLayout current_layout, vk::ImageLayout new_layout, vk::QueueFlagBits queue_type)
    {
        TransitionDependency dependency;
        switch (queue_type) {
            case vk::QueueFlagBits::eGraphics: {
                dependency = get_image_layout_transition_dependency<vk::QueueFlagBits::eGraphics>(current_layout, new_layout);
            } break;
            case vk::QueueFlagBits::eTransfer: {
                dependency = get_image_layout_transition_dependency<vk::QueueFlagBits::eTransfer>(current_layout, new_layout);
            } break;
        }
        return dependency;
    }

    template <vk::QueueFlagBits queue_type>
    constexpr TransitionDependency get_buffer_copy_dependency(GPUBufferUsage buffer_usage);

    template <>
    constexpr TransitionDependency get_buffer_copy_dependency<vk::QueueFlagBits::eTransfer>(GPUBufferUsage buffer_usage)
    {
        return std::make_tuple(
            vk::PipelineStageFlagBits2KHR::eAllTransfer,
            vk::AccessFlagBits2KHR::eTransferRead,
            vk::PipelineStageFlagBits2KHR::eAllTransfer,
            vk::AccessFlagBits2KHR::eTransferWrite);
    }

    template <>
    constexpr TransitionDependency get_buffer_copy_dependency<vk::QueueFlagBits::eGraphics>(GPUBufferUsage buffer_usage)
    {
        vk::PipelineStageFlags2 dst_stage;
        vk::AccessFlags2 access_flags;
        switch (buffer_usage) {
            case GPUBufferUsage::eVertex : {
                dst_stage = vk::PipelineStageFlagBits2::eVertexInput;
                access_flags = vk::AccessFlagBits2::eVertexAttributeRead;
            } break;
            case GPUBufferUsage::eIndex : {
                dst_stage = vk::PipelineStageFlagBits2::eVertexInput;
                access_flags = vk::AccessFlagBits2::eIndexRead;
            } break;
            case GPUBufferUsage::eUniform : {
                dst_stage = vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader;
                access_flags = vk::AccessFlagBits2::eShaderRead;
            } break;
            case GPUBufferUsage::eShaderStorage : {
                dst_stage = vk::PipelineStageFlagBits2::eVertexShader;
                access_flags = vk::AccessFlagBits2::eShaderRead;
            } break;
            case GPUBufferUsage::eIndirect : {
                dst_stage = vk::PipelineStageFlagBits2::eDrawIndirect;
                access_flags = vk::AccessFlagBits2::eIndirectCommandRead;
            } break;
            case GPUBufferUsage::eStaging : {
                dst_stage = vk::PipelineStageFlagBits2KHR::eAllGraphics;
                access_flags = vk::AccessFlagBits2KHR::eMemoryRead;
            } break;
        }
        return std::make_tuple(
            vk::PipelineStageFlagBits2KHR::eAllTransfer,
            vk::AccessFlagBits2KHR::eTransferWrite,
            dst_stage,
            access_flags);
    }

    constexpr TransitionDependency get_buffer_copy_dependency(GPUBufferUsage buffer_usage, vk::QueueFlagBits queue_type)
    {
        TransitionDependency dependency;
        switch (queue_type) {
            case vk::QueueFlagBits::eGraphics: {
                dependency = get_buffer_copy_dependency<vk::QueueFlagBits::eGraphics>(buffer_usage);
            } break;
            case vk::QueueFlagBits::eTransfer: {
                dependency = get_buffer_copy_dependency<vk::QueueFlagBits::eTransfer>(buffer_usage);
            } break;
        }
        return dependency;
    }

    void immediate_submit(VulkanContext * context, vk::QueueFlagBits queue_type, std::function<void(VulkanCommandBufferObject &)> && submit_func);
}