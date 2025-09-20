#pragma once

#include <vulkan/vulkan.hpp>
#include <functional>
#include "VulkanContext.h"

namespace lcf::render::vkutils {
    class ImageLayoutTransitionAssistant : public vk::ImageMemoryBarrier2
    {
    public:
        ImageLayoutTransitionAssistant();
        void setImage(vk::Image image, vk::ImageLayout current_layout = vk::ImageLayout::eUndefined);
        void transitTo(vk::CommandBuffer cmd, vk::ImageLayout new_layout);
    private:
        vk::Image m_image = nullptr;
    };

    class CopyAssistant
    {
    public:
        CopyAssistant(vk::CommandBuffer cmd);
        void copy(vk::Image src, vk::Image dst, vk::Extent2D src_size, vk::Extent2D dst_size, vk::Filter filter = vk::Filter::eNearest);
    private:
        vk::CommandBuffer m_cmd = nullptr;
    };

    using TransitionDependency = std::tuple<vk::PipelineStageFlags2, vk::AccessFlags2, vk::PipelineStageFlags2, vk::AccessFlags2>;

    constexpr TransitionDependency get_transition_dependency(vk::ImageLayout current_layout, vk::ImageLayout new_layout)
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

    void immediate_submit(VulkanContext * context, std::function<void()> && submit_func);
}