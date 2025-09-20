#include "VulkanFramebuffer.h"
#include "VulkanContext.h"

using namespace lcf::render;

lcf::render::VulkanFramebuffer::VulkanFramebuffer(VulkanContext *context) :
    m_context_p(context)
{
}

lcf::render::VulkanFramebuffer & lcf::render::VulkanFramebuffer::addColorAttachment2D()
{
    VulkanImage::UniquePointer color_image = VulkanImage::makeUnique();
    color_image->setImageType(vk::ImageType::e2D);
    m_color_attachments.emplace_back(std::make_pair(std::move(color_image), nullptr));
    return *this;
}

lcf::render::VulkanFramebuffer & lcf::render::VulkanFramebuffer::setDepthStencilAttachmentFormat(vk::Format depth_stencil_format)
{
    m_depth_stencil_attachment = DepthStencilAttachment{};
    m_depth_stencil_attachment->depth_stencil_image = VulkanImage::makeUnique();
    m_depth_stencil_attachment->depth_stencil_image->setImageType(vk::ImageType::e2D)
        .setFormat(depth_stencil_format);
    return *this;
}

bool lcf::render::VulkanFramebuffer::create()
{
    bool created = true;
    if (m_color_attachments.empty()) { this->addColorAttachment2D(); }
    for (auto & [color_image, color_image_view] : m_color_attachments) {
        color_image->setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment)
            .setFormat(m_color_format)
            .setExtent({m_extent.width, m_extent.height, 1})
            .setSamples(m_sample_count)
            .create(m_context_p);
        color_image_view = color_image->getDefaultView();
    }
    if (m_depth_stencil_attachment) {
        auto &[depth_stencil_image, depth_view, stencil_view] = *m_depth_stencil_attachment;
        depth_stencil_image->setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
            .setExtent({m_extent.width, m_extent.height, 1})
            .setSamples(m_sample_count)
            .create(m_context_p);
        depth_view = depth_stencil_image->getDefaultView(); //todo add method VulkanImage::getView(vk::ImageSubresourceRange), return nullptr if the format is not supported
    }
    if (m_sample_count > vk::SampleCountFlagBits::e1) {
        VulkanImage::UniquePointer msaa_color_image = VulkanImage::makeUnique();
        msaa_color_image->setImageType(vk::ImageType::e2D)
            .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment)
            .setFormat(m_color_format)
            .setExtent({m_extent.width, m_extent.height, 1})
            .setSamples(vk::SampleCountFlagBits::e1)
            .create(m_context_p);
        vk::ImageView msaa_color_view = msaa_color_image->getDefaultView();
        m_msaa_resolve_attachment = MSAAResolveAttachment{};
        m_msaa_resolve_attachment->msaa_resolve_image = std::move(msaa_color_image);
        m_msaa_resolve_attachment->msaa_resolve_view = msaa_color_view;
    }

    return created;
}

void lcf::render::VulkanFramebuffer::beginRendering()
{
    auto cmd = m_context_p->getCurrentCommandBuffer();
    std::vector<vk::RenderingAttachmentInfo> color_attachment_infos(m_color_attachments.size());
    for (uint32_t i = 0; i < m_color_attachments.size(); ++i) {
        auto & [color_image, color_image_view] = m_color_attachments[i];
        color_image->setInitialLayout(vk::ImageLayout::eUndefined)
            .transitLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
        color_attachment_infos[i].setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setImageView(color_image_view)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue({{ 0.1f, 0.1f, 0.1f, 1.0f }});
    }

    vk::RenderingAttachmentInfo depth_attachment_info;
    if (m_depth_stencil_attachment) {
        auto &[depth_stencil_image, depth_image_view, stencil_image_view] = *m_depth_stencil_attachment;
        depth_stencil_image->setInitialLayout(vk::ImageLayout::eUndefined)
            .transitLayout(cmd, vk::ImageLayout::eDepthStencilAttachmentOptimal);
        depth_attachment_info.setImageView(depth_image_view)
            .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue({{ 1.0f, 0u }});
    }
    if (m_msaa_resolve_attachment) {
        auto &[msaa_resolve_image, msaa_resolve_view, resolve_mode, src_index] = *m_msaa_resolve_attachment;
        msaa_resolve_image->setInitialLayout(vk::ImageLayout::eUndefined)
            .transitLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
        color_attachment_infos[src_index].setResolveMode(resolve_mode)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setResolveImageView(msaa_resolve_view);
    }
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea({ { 0, 0 }, m_extent })
        .setLayerCount(1)
        .setColorAttachments(color_attachment_infos)
        .setPDepthAttachment(&depth_attachment_info);
    cmd->beginRendering(rendering_info);
}

void lcf::render::VulkanFramebuffer::endRendering() const
{
    auto cmd = m_context_p->getCurrentCommandBuffer();
    cmd->endRendering();
}
