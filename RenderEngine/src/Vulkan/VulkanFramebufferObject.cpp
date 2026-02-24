#include "Vulkan/VulkanFramebufferObject.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/VulkanImageObject.h"
#include "log.h"

using namespace lcf::render;

bool VulkanFramebufferObject::create(VulkanContext *context_p, const VulkanFramebufferObjectCreateInfo &create_info)
{
    if (not context_p or not context_p->isCreated()) {
        std::runtime_error error("Invalid context");
        lcf_log_error(error.what());
        throw error;
    }
    m_max_extent = create_info.getMaxExtent();
    m_extent = std::min({m_max_extent, create_info.getExtent(), m_extent});

    vk::ImageUsageFlags color_attachment_usage = vk::ImageUsageFlagBits::eColorAttachment;
    if (create_info.isEnableMSAA()) {
        color_attachment_usage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
    }
    for (auto color_format : create_info.getColorFormats()) {
        auto image_sp = VulkanImageObject::makeShared();
        image_sp->setUsage(color_attachment_usage)
            .setExtent({m_max_extent.width, m_max_extent.height, 1u})
            .setSamples(create_info.getSampleCount())
            .setFormat(color_format)
            .create(context_p);
        m_color_attachments.emplace_back(image_sp);
    }
    if (m_color_attachments.empty()) {
        auto image_sp = VulkanImageObject::makeShared();
        image_sp->setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst)
            .setExtent({m_max_extent.width, m_max_extent.height, 1u})
            .setSamples(vk::SampleCountFlagBits::e1)
            .setFormat(vk::Format::eR8G8B8A8Srgb)
            .create(context_p);
        m_color_attachments.emplace_back(image_sp);
    }
    for (const auto & color_attachment : m_color_attachments) {
        m_layer_count = std::min(m_layer_count, color_attachment.getLayerCount());
    }
    if (create_info.hasDepthStencilFormat()) {
        auto image_sp = VulkanImageObject::makeShared();
        image_sp->setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
            .setExtent({m_extent.width, m_extent.height, 1})
            .setSamples(m_color_attachments.front().getImageSharedPointer()->getSamples())
            .setFormat(create_info.getDepthStencilFormat())
            .create(context_p);
        m_depth_stencil_attachment.emplace(image_sp);
        m_depth_stencil_attachment->setClearDepthStencilValue({1.0f, 0});
    }
    if (create_info.isEnableMSAA()) {
        auto image_sp = VulkanImageObject::makeShared();
        image_sp->setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment)
            .setFormat(m_color_attachments.front().getImageSharedPointer()->getFormat())
            .setExtent({m_extent.width, m_extent.height, 1})
            .setSamples(vk::SampleCountFlagBits::e1)
            .create(context_p);
        m_msaa_resolve_attachment.emplace(image_sp);
        m_msaa_resolve_attachment->setResolveMode(create_info.getResolveMode());
    }
    return true;
}

VulkanFramebufferObject & VulkanFramebufferObject::addColorAttachment(const Attachment &attachment)
{
    m_color_attachments.emplace_back(attachment);
    return *this;
}

void VulkanFramebufferObject::beginRendering(VulkanCommandBufferObject & cmd) noexcept
{
    std::vector<vk::RenderingAttachmentInfo> color_attachment_infos(m_color_attachments.size());
    for (int i = 0; i < m_color_attachments.size(); ++i) {
        auto & color_attachment = m_color_attachments[i];
        color_attachment.transitLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
        color_attachment_infos[i].setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setImageView(color_attachment.getImageView())
            .setLoadOp(color_attachment.getLoadOp())
            .setStoreOp(color_attachment.getStoreOp())
            .setClearValue(color_attachment.getClearValue());
    }
    vk::RenderingAttachmentInfo depth_stencil_attachment_info;
    if (m_depth_stencil_attachment) {
        auto & depth_stencil_attachment = *m_depth_stencil_attachment;
        depth_stencil_attachment.transitLayout(cmd, vk::ImageLayout::eDepthStencilAttachmentOptimal);
        depth_stencil_attachment_info.setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setImageView(depth_stencil_attachment.getImageView())
            .setLoadOp(depth_stencil_attachment.getLoadOp())
            .setStoreOp(depth_stencil_attachment.getStoreOp())
            .setClearValue(depth_stencil_attachment.getClearValue());
    }
    if (m_msaa_resolve_attachment) {
        auto & msaa_resolve_attachment = *m_msaa_resolve_attachment;
        msaa_resolve_attachment.transitLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
        color_attachment_infos[0].setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setResolveImageView(msaa_resolve_attachment.getImageView())
            .setResolveMode(msaa_resolve_attachment.getResolveMode());
    }
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea({ { 0, 0 }, m_extent })
        .setLayerCount(m_layer_count)
        .setColorAttachments(color_attachment_infos)
        .setPDepthAttachment(&depth_stencil_attachment_info);
    cmd.beginRendering(rendering_info);
}

void VulkanFramebufferObject::endRendering(VulkanCommandBufferObject & cmd) noexcept
{
    cmd.endRendering();
}

void lcf::render::VulkanFramebufferObject::setViewportAndScissor(VulkanCommandBufferObject &cmd) noexcept
{
    auto [w, h] = m_extent;
    vk::Viewport viewport;
    viewport.setX(0.0f).setY(h)
        .setWidth(static_cast<float>(w))
        .setHeight(-static_cast<float>(h))
        .setMinDepth(0.0f).setMaxDepth(1.0f);
    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 }).setExtent(m_extent);
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);
}
