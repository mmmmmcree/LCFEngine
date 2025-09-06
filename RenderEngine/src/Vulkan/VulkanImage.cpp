#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "vulkan_utililtie.h"

lcf::render::VulkanImage::VulkanImage(VulkanContext * context) :
    m_context_p(context)
{
}

void lcf::render::VulkanImage::create()
{
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

void lcf::render::VulkanImage::create(const vk::ImageCreateInfo &info)
{
    m_flags = info.flags;
    m_image_type = info.imageType;
    m_format = info.format;
    m_extent = info.extent;
    m_array_layers = info.arrayLayers;
    m_samples = info.samples;
    m_tiling = info.tiling;
    m_usage = info.usage;
    m_layout = info.initialLayout;
    m_image = m_context_p->getMemoryAllocator()->createImage(info);
}

void lcf::render::VulkanImage::setData(const Image &image)
{
    this->setExtent({ static_cast<uint32_t>(image.getWidth()), static_cast<uint32_t>(image.getHeight()), 1u })
        .setFormat(vk::Format::eR8G8B8A8Unorm);
    m_usage |= vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    this->create();
    
    VulkanBuffer staging_buffer(m_context_p);
    staging_buffer.setUsageFlags(vk::BufferUsageFlagBits::eTransferSrc);
    staging_buffer.setData(image.getData(), image.getSizeInBytes());
    vkutils::immediate_submit(m_context_p, [&] {
        auto cmd = m_context_p->getCurrentCommandBuffer();
        vk::BufferImageCopy region;
        region.setImageSubresource({ this->getAspectFlags(), 0, 0, 1 })
        .setImageOffset({ 0, 0, 0 })
        .setImageExtent(m_extent);
        this->transitLayout(vk::ImageLayout::eTransferDstOptimal);
        cmd.copyBufferToImage(staging_buffer.getHandle(), m_image.get(), vk::ImageLayout::eTransferDstOptimal, region);
        this->transitLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    });
}

void lcf::render::VulkanImage::transitLayout(vk::ImageLayout new_layout)
{
    auto cmd = m_context_p->getCurrentCommandBuffer();
    vk::ImageMemoryBarrier2 barrier;
    barrier.setImage(m_image.get())
        .setOldLayout(m_layout)
        .setNewLayout(new_layout)
        .setSubresourceRange({ this->getAspectFlags(), 0u, this->getMipLevelCount(), 0u, this->getArrayLayerCount() });
    if (barrier.oldLayout == vk::ImageLayout::eUndefined and barrier.newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.setSrcAccessMask({})
            .setDstAccessMask(vk::AccessFlagBits2KHR::eTransferWrite)
            .setSrcStageMask(vk::PipelineStageFlagBits2KHR::eTopOfPipe)
            .setDstStageMask(vk::PipelineStageFlagBits2KHR::eTransfer);
    } else if (barrier.oldLayout == vk::ImageLayout::eTransferDstOptimal and barrier.newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlagBits2KHR::eTransferWrite)
            .setDstAccessMask(vk::AccessFlagBits2KHR::eShaderRead)
            .setSrcStageMask(vk::PipelineStageFlagBits2KHR::eTransfer)
            .setDstStageMask(vk::PipelineStageFlagBits2KHR::eFragmentShader);
    } else {
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2KHR::eAllCommands)
        .setSrcAccessMask(vk::AccessFlagBits2::eMemoryWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eAllCommands)
        .setDstAccessMask(vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead);
    }
    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(barrier);
    cmd.pipelineBarrier2(dependency_info);
    m_layout = new_layout;
}

vk::UniqueImageView lcf::render::VulkanImage::generateView(const vk::ImageSubresourceRange & subresource_range) const
{
    vk::ImageViewCreateInfo view_info;
    view_info.setImage(m_image.get())
        .setViewType(this->deduceImageViewType())
        .setFormat(m_format)
        .setSubresourceRange(subresource_range);
    return m_context_p->getDevice().createImageViewUnique(view_info);
}

vk::ImageView lcf::render::VulkanImage::getDefaultView() const
{
    vk::ImageSubresourceRange range = { this->getAspectFlags(), 0, this->getMipLevelCount(), 0, this->getArrayLayerCount() };
    auto it = m_view_map.find(range);
    if (it == m_view_map.end()) {
        return m_view_map.emplace(std::make_pair(range, this->generateView(range))).first->second.get();
    }
    return it->second.get();
}

uint32_t lcf::render::VulkanImage::getMipLevelCount() const
{
    if (not m_mipmapped) { return 1u; }
    return std::floor(std::log2(std::max(m_extent.width, m_extent.height))) + 1u;
}

vk::ImageAspectFlags lcf::render::VulkanImage::getAspectFlags() const
{
    vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor;
    if (m_format == vk::Format::eD32Sfloat) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth;
    } else if (m_format == vk::Format::eD24UnormS8Uint or m_format == vk::Format::eD32SfloatS8Uint) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    }
    return aspect_mask;
}

vk::ImageViewType lcf::render::VulkanImage::deduceImageViewType() const
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
