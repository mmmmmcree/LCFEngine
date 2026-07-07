#include "vk_core/pipeline/graphics/Framebuffer.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/memory/enums.h"

namespace lcf::vkc {

std::error_code Framebuffer::create(const MemoryAllocator & allocator, const FramebufferCreateInfo & info) noexcept
{
    // m_extent = vk::Extent2D { info.getWidth(), info.getHeight() };
    // m_layer_count = info.getLayerCount();
    // const vk::ImageViewType view_type = m_layer_count > 1u ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;

    // vk::Device device = allocator.getDevice();
    // MemoryAllocationInfo alloc_info;
    // alloc_info.setAccess(MemoryAccess::eDeviceLocal);

    // m_attachments.clear();
    // m_attachments.reserve(info.getAttachmentInfos().size());
    // for (const auto & attachment_info : info.getAttachmentInfos()) {
    //     vk::ImageCreateInfo image_info;
    //     image_info.setImageType(vk::ImageType::e2D)
    //         .setFormat(attachment_info.m_format)
    //         .setExtent({m_extent.width, m_extent.height, 1u})
    //         .setMipLevels(1u)
    //         .setArrayLayers(m_layer_count)
    //         .setSamples(attachment_info.m_samples)
    //         .setTiling(vk::ImageTiling::eOptimal)
    //         .setUsage(attachment_info.m_usage)
    //         .setInitialLayout(vk::ImageLayout::eUndefined);
    //     auto expected_image = allocator.createImage(image_info, alloc_info);
    //     if (not expected_image) { return expected_image.error(); }

    //     Attachment attachment;
    //     attachment.m_image = std::move(expected_image.value());
    //     attachment.m_format = attachment_info.m_format;
    //     attachment.m_samples = attachment_info.m_samples;
    //     vk::ImageViewCreateInfo view_info;
    //     view_info.setImage(attachment.m_image.handle())
    //         .setViewType(view_type)
    //         .setFormat(attachment_info.m_format)
    //         .setSubresourceRange({attachment_info.m_aspect, 0u, 1u, 0u, m_layer_count});
    //     try {
    //         attachment.m_view = device.createImageViewUnique(view_info);
    //     } catch (const vk::SystemError & e) {
    //         return e.code();
    //     }
    //     m_attachments.emplace_back(std::move(attachment));
    // }
    return {};
}

} // namespace lcf::vkc
