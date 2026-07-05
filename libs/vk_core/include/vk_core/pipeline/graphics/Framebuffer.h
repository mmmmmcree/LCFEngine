#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <span>
#include <ranges>
#include <system_error>
#include "vk_core/memory/Image.h"

namespace lcf::vkc {

class MemoryAllocator;

class FramebufferCreateInfo;


class Framebuffer
{
    using Self = Framebuffer;
    struct Attachment
    {
        Image m_image;
        vk::UniqueImageView m_view;
        vk::Format m_format = vk::Format::eUndefined;
        vk::SampleCountFlagBits m_samples = vk::SampleCountFlagBits::e1;
        vk::ImageLayout m_current_layout = vk::ImageLayout::eUndefined;
    };
    using AttachmentList = std::vector<Attachment>;
public:
    ~Framebuffer() noexcept = default;
    Framebuffer() noexcept = default;
    Framebuffer(const Self &) noexcept = delete;
    Framebuffer(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(const MemoryAllocator & allocator, const FramebufferCreateInfo & info) noexcept;
    auto viewImageViews() const noexcept { return m_attachments | std::views::transform([](const Attachment & attachment) { return attachment.m_view.get(); }); }
    const vk::Extent2D & getExtent() const noexcept { return m_extent; }
    const uint32_t & getLayerCount() const noexcept { return m_layer_count; }
    size_t getAttachmentCount() const noexcept { return m_attachments.size(); }
    const vk::ImageView & getView(size_t index) const noexcept { return m_attachments[index].m_view.get(); }
    const vk::Format & getFormat(size_t index) const noexcept { return m_attachments[index].m_format; }
    const vk::SampleCountFlagBits & getSamples(size_t index) const noexcept { return m_attachments[index].m_samples; }
    const vk::ImageLayout & getLayout(size_t index) const noexcept { return m_attachments[index].m_current_layout; }
    void setLayout(size_t index, vk::ImageLayout layout) noexcept { m_attachments[index].m_current_layout = layout; }
private:
    vk::Extent2D m_extent;
    uint32_t m_layer_count = 1u;
    AttachmentList m_attachments;
};
//todo move AttachmentInfo outside of FramebufferCreateInfo
class FramebufferCreateInfo
{
    using Self = FramebufferCreateInfo;
public:
    struct AttachmentInfo
    {
        vk::Format m_format = vk::Format::eUndefined;
        vk::ImageUsageFlags m_usage = {};
        vk::ImageAspectFlags m_aspect = {};
        vk::SampleCountFlagBits m_samples = vk::SampleCountFlagBits::e1;
    };
private:
    using AttachmentInfoList = std::vector<AttachmentInfo>;
public:
    ~FramebufferCreateInfo() noexcept = default;
    FramebufferCreateInfo() = default;
    FramebufferCreateInfo(const Self & other) = default;
    FramebufferCreateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & setExtent(uint32_t width, uint32_t height, uint32_t layers = 1u) noexcept
    {
        m_width = width;
        m_height = height;
        m_layer_count = layers;
        return *this;
    }
    Self & addAttachment(
        vk::Format format,
        vk::ImageUsageFlags usage,
        vk::ImageAspectFlags aspect,
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1) noexcept
    {
        m_attachment_infos.emplace_back(format, usage, aspect, samples);
        return *this;
    }
    Self & addColorAttachment(vk::Format format, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1) noexcept
    {
        return this->addAttachment(format, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageAspectFlagBits::eColor, samples);
    }
    Self & addDepthStencilAttachment(vk::Format format, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1) noexcept
    {
        return this->addAttachment(format, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth, samples);
    }
    const uint32_t & getWidth() const noexcept { return m_width; }
    const uint32_t & getHeight() const noexcept { return m_height; }
    const uint32_t & getLayerCount() const noexcept { return m_layer_count; }
    std::span<const AttachmentInfo> getAttachmentInfos() const noexcept { return m_attachment_infos; }
private:
    uint32_t m_width = 0u;
    uint32_t m_height = 0u;
    uint32_t m_layer_count = 1u;
    AttachmentInfoList m_attachment_infos;
};

} // namespace lcf::vkc
