#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanBufferObject.h"
#include "vulkan_utililtie.h"

using namespace lcf::render;

void lcf::render::VulkanImage::create(VulkanContext * context_p)
{
    m_context_p = context_p;
    vk::ImageCreateInfo image_info;
    image_info.setFlags(m_flags)
        .setImageType(m_image_type)
        .setFormat(m_format)
        .setExtent(m_extent)
        .setMipLevels(this->getMipLevelCount())
        .setArrayLayers(m_array_layers)
        .setSamples(m_samples)
        .setTiling(m_tiling)
        .setUsage(m_usage)
        .setInitialLayout(m_layout)
        .setSharingMode(vk::SharingMode::eExclusive);
    m_image = m_context_p->getMemoryAllocator()->createImage(image_info);
}

void lcf::render::VulkanImage::create(VulkanContext *context_p, vk::Image external_image)
{
    m_context_p = context_p;
    m_image = external_image;
}

void lcf::render::VulkanImage::create(VulkanContext * context_p, const Image &image)
{
    this->setExtent({ static_cast<uint32_t>(image.getWidth()), static_cast<uint32_t>(image.getHeight()), 1u })
        .setFormat(vk::Format::eR8G8B8A8Unorm);
    m_usage |= vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    this->create(context_p);
    
    VulkanBufferObject staging_buffer;
    staging_buffer.setUsage(GPUBufferUsage::eStaging)
        .setSize(image.getSizeInBytes())
        .create(m_context_p);
    staging_buffer.addWriteSegment({std::span(static_cast<const std::byte *>(image.getData()), image.getSizeInBytes())});
    staging_buffer.commitWriteSegments();
    vkutils::immediate_submit(m_context_p, [&] {
        auto cmd = m_context_p->getCurrentCommandBuffer();
        vk::BufferImageCopy region;
        region.setImageSubresource({ this->getAspectFlags(), 0, 0, 1 })
        .setImageOffset({ 0, 0, 0 })
        .setImageExtent(m_extent);
        this->transitLayout(cmd, vk::ImageLayout::eTransferDstOptimal);
        cmd->copyBufferToImage(staging_buffer.getHandle(), this->getHandle(), vk::ImageLayout::eTransferDstOptimal, region);
        this->transitLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    });
}

void lcf::render::VulkanImage::transitLayout(VulkanCommandBufferObject * cmd_p, vk::ImageLayout new_layout)
{
    this->transitLayout(cmd_p, this->getFullResourceRange(), new_layout);
}

void lcf::render::VulkanImage::transitLayout(VulkanCommandBufferObject *cmd_p, const vk::ImageSubresourceRange &subresource_range, vk::ImageLayout new_layout)
{
    vk::ImageMemoryBarrier2 barrier;
    auto [src_stage, src_access, dst_stage, dst_access] = vkutils::get_transition_dependency(m_layout, new_layout);
    barrier.setImage(this->getHandle())
        .setOldLayout(m_layout)
        .setNewLayout(new_layout)
        .setSubresourceRange(subresource_range)
        .setSrcStageMask(src_stage)
        .setSrcAccessMask(src_access)
        .setDstStageMask(dst_stage)
        .setDstAccessMask(dst_access);
    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(barrier);
    cmd_p->pipelineBarrier2(dependency_info);
    m_layout = new_layout;
}

vk::Image lcf::render::VulkanImage::getHandle() const
{
    return std::visit([](const auto& img) -> vk::Image {
        if constexpr (std::is_same_v<std::decay_t<decltype(img)>, vk::Image>) { return img; }
        else { return img->getHandle(); }
    }, m_image);
}

vk::UniqueImageView lcf::render::VulkanImage::generateView(const vk::ImageSubresourceRange & subresource_range) const
{
    vk::ImageViewCreateInfo view_info;
    view_info.setImage(this->getHandle())
        .setViewType(this->deduceImageViewType())
        .setFormat(m_format)
        .setSubresourceRange(subresource_range);
    return m_context_p->getDevice().createImageViewUnique(view_info);
}

vk::ImageView lcf::render::VulkanImage::getDefaultView() const
{
    return this->getView(vk::ImageSubresourceRange(this->getAspectFlags(), 0, this->getMipLevelCount(), 0, this->getArrayLayerCount()));
}

vk::ImageView lcf::render::VulkanImage::getView(const vk::ImageSubresourceRange &subresource_range) const
{
    auto it = m_view_map.find(subresource_range);
    if (it == m_view_map.end()) {
        return m_view_map.emplace(std::make_pair(subresource_range, this->generateView(subresource_range))).first->second.get();
    }
    return it->second.get();
}

uint32_t lcf::render::VulkanImage::getMipLevelCount() const
{
    if (not m_mipmapped) { return 1u; }
    return std::floor(std::log2(std::max(m_extent.width, m_extent.height))) + 1u;
}

vk::ImageAspectFlags lcf::render::VulkanImage::getAspectFlags() const noexcept
{
    vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor;
    if (m_format == vk::Format::eD32Sfloat) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth;
    } else if (m_format == vk::Format::eD24UnormS8Uint or m_format == vk::Format::eD32SfloatS8Uint) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    }
    return aspect_mask;
}

vk::ImageViewType lcf::render::VulkanImage::deduceImageViewType() const noexcept
{
    vk::ImageViewType view_type = {};
    switch (m_image_type) {
        case vk::ImageType::e1D : {
            view_type = m_array_layers > 1 ? vk::ImageViewType::e1DArray : vk::ImageViewType::e1D;
        } break;
        case vk::ImageType::e2D : {
            if (m_flags & vk::ImageCreateFlagBits::eCubeCompatible) {
                view_type = m_array_layers > 6 ? vk::ImageViewType::eCubeArray : vk::ImageViewType::eCube;
            } else {
                view_type = m_array_layers > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
            }
        } break;
        case vk::ImageType::e3D : {
            view_type = vk::ImageViewType::e3D;
        } break;
    }
    return view_type;
}

vk::ImageSubresourceRange lcf::render::VulkanImage::getFullResourceRange() const noexcept
{
    return vk::ImageSubresourceRange(this->getAspectFlags(), 0, this->getMipLevelCount(), 0, this->getArrayLayerCount());
}

// - VulkanAttachment

VulkanAttachment::VulkanAttachment(VulkanImage * image, uint32_t mip_level, uint32_t layer, uint32_t layer_count) :
    m_image_p(image),
    m_mip_level(mip_level),
    m_layer(layer),
    m_layer_count(layer_count)
{
}

lcf::render::VulkanAttachment::VulkanAttachment(const VulkanAttachment &other) :
    m_image_p(other.m_image_p),
    m_mip_level(other.m_mip_level),
    m_layer(other.m_layer),
    m_layer_count(other.m_layer_count)
{
}

lcf::render::VulkanAttachment::VulkanAttachment(VulkanAttachment &&other) noexcept :
    m_image_p(other.m_image_p),
    m_mip_level(other.m_mip_level),
    m_layer(other.m_layer),
    m_layer_count(other.m_layer_count)
{
}

void VulkanAttachment::blitTo(VulkanCommandBufferObject *cmd_p, VulkanAttachment &dst, vk::Filter filter, const Offset3DPair &src_offsets, const Offset3DPair &dst_offsets)
{
    auto dst_image_p = dst.getImagePtr();
    m_image_p->transitLayout(cmd_p, vk::ImageLayout::eTransferSrcOptimal);
    dst_image_p->transitLayout(cmd_p, vk::ImageLayout::eTransferDstOptimal);
    vk::ImageBlit2 blit_region2 = {};
    blit_region2.setSrcSubresource(vk::ImageSubresourceLayers(m_image_p->getAspectFlags(), m_mip_level, m_layer, m_layer_count))
        .setDstSubresource(vk::ImageSubresourceLayers(dst_image_p->getAspectFlags(), dst.getMipLevel(), dst.getLayer(), dst.getLayerCount()));
    memcpy(blit_region2.srcOffsets, &src_offsets, sizeof(Offset3DPair));
    memcpy(blit_region2.dstOffsets, &dst_offsets, sizeof(Offset3DPair));
    vk::BlitImageInfo2 blit_info = {};
    blit_info.setSrcImage(m_image_p->getHandle())
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(dst_image_p->getHandle())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setRegions(blit_region2)
        .setFilter(filter);
    cmd_p->blitImage2(blit_info);
}

void lcf::render::VulkanAttachment::copyTo(VulkanCommandBufferObject *cmd_p, VulkanAttachment &dst, const vk::Offset3D &src_offset, const vk::Offset3D &dst_offset, const vk::Extent3D &extent)
{
    auto dst_image_p = dst.getImagePtr();
    m_image_p->transitLayout(cmd_p, vk::ImageLayout::eTransferSrcOptimal);
    dst_image_p->transitLayout(cmd_p, vk::ImageLayout::eTransferDstOptimal);
    vk::ImageCopy2 copy_region2 = {};
    copy_region2.setSrcSubresource(vk::ImageSubresourceLayers(m_image_p->getAspectFlags(), m_mip_level, m_layer, m_layer_count))
        .setDstSubresource(vk::ImageSubresourceLayers(dst_image_p->getAspectFlags(), dst.getMipLevel(), dst.getLayer(), dst.getLayerCount()))
        .setSrcOffset(src_offset)
        .setDstOffset(dst_offset)
        .setExtent(extent);
    vk::CopyImageInfo2 copy_info = {};
    copy_info.setSrcImage(m_image_p->getHandle())
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(dst_image_p->getHandle())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setRegions(copy_region2);
    cmd_p->copyImage2(copy_info);
}

vk::ImageSubresourceRange lcf::render::VulkanAttachment::getSubresourceRange() const noexcept
{
    return vk::ImageSubresourceRange(m_image_p->getAspectFlags(), m_mip_level, 1, m_layer, m_layer_count);
}

vk::ImageView lcf::render::VulkanAttachment::getImageView() const noexcept
{
    return m_image_p->getView(this->getSubresourceRange());
}

void lcf::render::VulkanAttachment::transitLayout(VulkanCommandBufferObject *cmd_p, vk::ImageLayout new_layout)
{
    m_image_p->transitLayout(cmd_p, this->getSubresourceRange(), new_layout);
}

vk::ImageLayout lcf::render::VulkanAttachment::getLayout() const noexcept
{
    return m_image_p->getLayout();
}
