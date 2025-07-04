#include "vulkan_utililtie.h"

lcf::render::vkutils::ImageLayoutTransitionAssistant::ImageLayoutTransitionAssistant()
{
    this->setSrcStageMask(vk::PipelineStageFlagBits2KHR::eAllCommands)
        .setSrcAccessMask(vk::AccessFlagBits2::eMemoryWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eAllCommands)
        .setDstAccessMask(vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead)
        .setOldLayout(vk::ImageLayout::eUndefined);
}

void lcf::render::vkutils::ImageLayoutTransitionAssistant::setImage(vk::Image image, vk::ImageLayout current_layout)
{
    vk::ImageMemoryBarrier2::setImage(image)
        .setOldLayout(current_layout);
}

void lcf::render::vkutils::ImageLayoutTransitionAssistant::transitTo(vk::CommandBuffer cmd, vk::ImageLayout new_layout)
{
    vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor;
    this->setNewLayout(new_layout)
        .setSubresourceRange({aspect_mask, 0, 1, 0, 1});
    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(*static_cast<const vk::ImageMemoryBarrier2 *>(this));
    cmd.pipelineBarrier2(dependency_info);
}

lcf::render::vkutils::CopyAssistant::CopyAssistant(vk::CommandBuffer cmd) : m_cmd(cmd)
{
}

void lcf::render::vkutils::CopyAssistant::copy(vk::Image src, vk::Image dst, vk::Extent2D src_size, vk::Extent2D dst_size, vk::Filter filter)
{
    vk::ImageBlit2 blit_region;
    blit_region.setSrcSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
        .setDstSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1});
    blit_region.srcOffsets[1] = vk::Offset3D{static_cast<int32_t>(src_size.width), static_cast<int32_t>(src_size.height), 1};
    blit_region.dstOffsets[1] = vk::Offset3D{static_cast<int32_t>(dst_size.width), static_cast<int32_t>(dst_size.height), 1};
    vk::BlitImageInfo2 blit_info;
    blit_info.setSrcImage(src)
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(dst)
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setRegions(blit_region)
        .setFilter(filter);
    m_cmd.blitImage2(blit_info);
}

void lcf::render::vkutils::immediate_submit(VulkanContext *context, std::function<void(vk::CommandBuffer)> &&submit_func)
{
    auto device = context->getDevice();
    vk::FenceCreateInfo fence_info;
    vk::Fence fence = device.createFence(fence_info);
    vk::CommandBufferAllocateInfo cmd_alloc_info(context->getCommandPool(), vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer cmd = device.allocateCommandBuffers(cmd_alloc_info).front();
    vk::CommandBufferBeginInfo cmd_begin_info;
    cmd_begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(cmd_begin_info);
    submit_func(cmd);
    cmd.end();
    vk::SubmitInfo submit_info;
    submit_info.setCommandBuffers(cmd);
    vk::Queue queue = context->getQueue(vk::QueueFlagBits::eGraphics);
    queue.submit(submit_info, fence);
    if (device.waitForFences(fence, true, UINT64_MAX) != vk::Result::eSuccess) {
        throw std::runtime_error("lcf::render::vkutils::immediate_submit: waitForFences failed");
    }
    device.destroyFence(fence);
    device.freeCommandBuffers(context->getCommandPool(), cmd);
}
