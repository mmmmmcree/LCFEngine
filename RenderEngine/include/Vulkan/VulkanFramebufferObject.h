#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanImage.h"
#include "VulkanCommandBufferObject.h"
#include <optional>

namespace lcf::render {
    class VulkanContext;

    struct VulkanFramebufferObjectCreateInfo;

    class VulkanFramebufferObject
    {
        using Self = VulkanFramebufferObject;
    public:
        class Attachment : public VulkanAttachment
        {
            using Self = Attachment;
        public:
            using VulkanAttachment::VulkanAttachment;
            Attachment(const VulkanAttachment & attachment) : VulkanAttachment(attachment) {}
            Self & setClearColorValue(const vk::ClearColorValue & clear_value) noexcept { m_clear_value = clear_value; return *this; }
            Self & setClearDepthStencilValue(const vk::ClearDepthStencilValue & clear_value) noexcept { m_clear_value = clear_value; return *this; }
            Self & setLoadOp(vk::AttachmentLoadOp load_op) noexcept { m_load_op = load_op; return *this; }
            Self & setStoreOp(vk::AttachmentStoreOp store_op) noexcept { m_store_op = store_op; return *this; }
            Self & setResolveMode(vk::ResolveModeFlagBits resolve_mode) noexcept { m_resolve_mode = resolve_mode; return *this; }
            const vk::ClearValue & getClearValue() const noexcept { return m_clear_value; }
            vk::AttachmentLoadOp getLoadOp() const noexcept { return m_load_op; }
            vk::AttachmentStoreOp getStoreOp() const noexcept { return m_store_op; }
            vk::ResolveModeFlagBits getResolveMode() const noexcept { return m_resolve_mode; }
        private: 
            vk::ClearValue m_clear_value = {};
            vk::AttachmentLoadOp m_load_op = vk::AttachmentLoadOp::eClear;
            vk::AttachmentStoreOp m_store_op = vk::AttachmentStoreOp::eStore;
            vk::ResolveModeFlagBits m_resolve_mode = vk::ResolveModeFlagBits::eNone;
        };
        using OptionalAttachment = std::optional<Attachment>;
        using AttachmentList = std::vector<Attachment>;
        VulkanFramebufferObject() = default;
        bool create(VulkanContext * context_p, const VulkanFramebufferObjectCreateInfo & create_info);
        Self & addColorAttachment(const Attachment & attachment);
        Self & setExtent(vk::Extent2D extent) noexcept { m_extent = std::min(m_max_extent, extent); return *this; }
        Attachment & getMainColorAttachment() noexcept { return m_color_attachments.front(); }
        OptionalAttachment getColorAttachment(uint32_t index) noexcept { return m_color_attachments.size() > index? std::make_optional(m_color_attachments[index]) : std::nullopt; }
        OptionalAttachment getDepthStencilAttachment() noexcept { return m_depth_stencil_attachment; }
        OptionalAttachment getMSAAResolveAttachment() noexcept { return m_msaa_resolve_attachment; }
        void beginRendering(VulkanCommandBufferObject * cmd_p);
        void endRendering(VulkanCommandBufferObject * cmd_p);
    private:
        vk::Extent2D m_max_extent = {-1u, -1u};
        vk::Extent2D m_extent = {-1u, -1u};
        vk::Format m_depth_stencil_format = vk::Format::eUndefined;
        AttachmentList m_color_attachments;
        OptionalAttachment m_depth_stencil_attachment;
        OptionalAttachment m_msaa_resolve_attachment;
    };

    struct VulkanFramebufferObjectCreateInfo
    {
        using Self = VulkanFramebufferObjectCreateInfo;
        using FormatList = std::vector<vk::Format>;
        VulkanFramebufferObjectCreateInfo(vk::Extent2D max_extent = {800u, 600u},
            vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1,
            vk::Format depth_stencil_format = vk::Format::eUndefined,
            vk::ResolveModeFlagBits resolve_mode = vk::ResolveModeFlagBits::eNone) :
            m_max_extent(max_extent),
            m_depth_stencil_format(depth_stencil_format),
            m_resolve_mode(resolve_mode) {}
        Self & setMaxExtent(vk::Extent2D max_extent) noexcept { m_max_extent = max_extent; return *this; }
        Self & addColorFormat(vk::Format format) { m_color_formats.emplace_back(format); return *this; }
        Self & addColorFormats(std::span<const vk::Format> formats) { m_color_formats.insert(m_color_formats.end(), formats.begin(), formats.end()); return *this; }
        Self & setSampleCount(vk::SampleCountFlagBits sample_count) noexcept { m_sample_count = sample_count; return *this; }
        Self & setDepthStencilFormat(vk::Format depth_stencil_format) noexcept { m_depth_stencil_format = depth_stencil_format; return *this; }
        Self & setResolveMode(vk::ResolveModeFlagBits resolve_mode) noexcept { m_resolve_mode = resolve_mode; return *this; }
        const vk::Extent2D & getMaxExtent() const noexcept { return m_max_extent; }
        const FormatList & getColorFormats() const noexcept { return m_color_formats; }
        vk::SampleCountFlagBits getSampleCount() const noexcept { return m_sample_count; }
        bool hasDepthStencilFormat() const noexcept { return m_depth_stencil_format != vk::Format::eUndefined; }
        const vk::Format & getDepthStencilFormat() const noexcept { return m_depth_stencil_format; }
        bool isEnableMSAA() const noexcept { return m_resolve_mode != vk::ResolveModeFlagBits::eNone; }
        const vk::ResolveModeFlagBits & getResolveMode() const noexcept { return m_resolve_mode; }
    //- properties
        vk::Extent2D m_max_extent;
        FormatList m_color_formats;
        vk::SampleCountFlagBits m_sample_count;
        vk::Format m_depth_stencil_format;
        vk::ResolveModeFlagBits m_resolve_mode;
    };
}