#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanImage.h"
#include "PointerDefs.h"
#include <vector>
#include <optional>

namespace lcf::render {
    class VulkanContext;

    class VulkanFramebuffer : public PointerDefs<VulkanFramebuffer>
    {
        using Self = VulkanFramebuffer;
        using ColorAttachment = std::pair<VulkanImage::UniquePointer, vk::ImageView>;
        using ColorAttachmentList = std::vector<ColorAttachment>;
        struct DepthStencilAttachment
        {
            VulkanImage::UniquePointer depth_stencil_image;
            vk::ImageView depth_view = nullptr;
            vk::ImageView stencil_view = nullptr;
        };
        struct MSAAResolveAttachment
        {
            VulkanImage::UniquePointer msaa_resolve_image;
            vk::ImageView msaa_resolve_view = nullptr;
            vk::ResolveModeFlagBits resolve_mode = vk::ResolveModeFlagBits::eAverage;
            size_t color_attachment_src_index = 0u;
        };
    public:
        using Attachment = std::pair<VulkanImage *, vk::ImageView>;
        VulkanFramebuffer(VulkanContext * context);
        Self & setSampleCount(vk::SampleCountFlagBits sample_count) { m_sample_count = sample_count; return *this; }
        Self & setColorAttachmentFormat(vk::Format format) { m_color_format = format; return *this; }
        Self & setExtent(vk::Extent2D extent) { m_extent = extent; return *this; }
        Self & addColorAttachment2D();
        Self & setDepthStencilAttachmentFormat(vk::Format depth_stencil_format);
        Attachment getColorAttachment(size_t index = 0) const { return std::make_pair(m_color_attachments[index].first.get(), m_color_attachments[index].second); }
        Attachment getDepthAttachment() const { return std::make_pair(m_depth_stencil_attachment->depth_stencil_image.get(), m_depth_stencil_attachment->depth_view); }
        Attachment getStencilAttachment() const { return std::make_pair(m_depth_stencil_attachment->depth_stencil_image.get(), m_depth_stencil_attachment->stencil_view); }
        Attachment getMSAAResolveAttachment() const { return m_msaa_resolve_attachment? std::make_pair(m_msaa_resolve_attachment->msaa_resolve_image.get(), m_msaa_resolve_attachment->msaa_resolve_view) : Attachment{ nullptr, nullptr }; }
        bool create();
        void beginRendering();
        void endRendering() const;
    private:
        VulkanContext * m_context;
        vk::Extent2D m_extent = {};
        vk::SampleCountFlagBits m_sample_count = vk::SampleCountFlagBits::e1;
        vk::Format m_color_format = vk::Format::eR8G8B8A8Srgb;
        ColorAttachmentList m_color_attachments;
        std::optional<DepthStencilAttachment> m_depth_stencil_attachment;
        std::optional<MSAAResolveAttachment> m_msaa_resolve_attachment;
    };
}