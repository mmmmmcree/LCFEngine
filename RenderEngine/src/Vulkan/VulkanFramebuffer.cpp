#include "VulkanFramebuffer.h"

lcf::render::VulkanFramebuffer::VulkanFramebuffer(VulkanContext *context) :
    m_context(context)
{
}

lcf::render::VulkanFramebuffer & lcf::render::VulkanFramebuffer::addColorAttachment(vk::ImageType image_type, bool mipmapped, uint32_t array_layers)
{
    VulkanImage::UniquePointer color_image = VulkanImage::makeUnique(m_context);
    color_image->setImageType(image_type)
        .setMipmapped(mipmapped)
        .setArrayLayers(array_layers);
    m_color_attachments.emplace_back(std::make_pair(std::move(color_image), nullptr));
    return *this;
}

lcf::render::VulkanFramebuffer & lcf::render::VulkanFramebuffer::setDepthStencilAttachmentFormat(vk::Format depth_stencil_format)
{
    m_depth_stencil_attachment = DepthStencilAttachment{};
    m_depth_stencil_attachment->depth_stencil_image = VulkanImage::makeUnique(m_context);
    m_depth_stencil_attachment->depth_stencil_image->setImageType(vk::ImageType::e2D)
        .setFormat(depth_stencil_format);
    return *this;
}

bool lcf::render::VulkanFramebuffer::create()
{
    bool created = true;
    if (m_color_attachments.empty()) { this->addColorAttachment(vk::ImageType::e2D); }
    for (auto & [color_image, color_image_view] : m_color_attachments) {
        color_image->setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment)
            .setFormat(m_color_format)
            .setSamples(m_sample_count)
            .setExtent({m_extent.width, m_extent.height, 1})
            .create();
        color_image_view = color_image->getDefaultView();
    }
    if (m_depth_stencil_attachment) {
        auto &[depth_stencil_image, depth_view, stencil_view] = *m_depth_stencil_attachment;
        depth_stencil_image->setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
            .setExtent({m_extent.width, m_extent.height, 1})
            .setSamples(m_sample_count)
            .create();
        depth_view = depth_stencil_image->getDefaultView(); //todo add method VulkanImage::getView(vk::ImageSubresourceRange), return nullptr if the format is not supported
    }
    return created;
}