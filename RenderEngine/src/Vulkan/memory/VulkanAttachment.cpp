#include "Vulkan/VulkanAttachment.h"
#include "Vulkan/VulkanImageObject.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "log.h"

using namespace lcf::render;

VulkanAttachment::VulkanAttachment(VulkanImageObject & image) :
    VulkanAttachment(image, 0, 0, image.getArrayLayerCount())
{
}

VulkanAttachment::VulkanAttachment(VulkanImageObject & image, uint32_t mip_level, uint32_t layer, uint32_t layer_count) :
    m_proxy_sp(image.m_proxy_sp),
    m_mip_level(mip_level),
    m_layer(layer),
    m_layer_count(layer_count)
{
    if (not this->isValid()) {
        std::runtime_error error("Invalid image for attachment");
        lcf_log_error(error.what());
        throw error;
    }
    bool is_layer_valid = m_layer + m_layer_count <= m_proxy_sp->getArrayLayerCount();
    bool is_mip_level_valid = m_mip_level < m_proxy_sp->getMipLevelCount();
    if (not is_layer_valid or not is_mip_level_valid) {
        std::runtime_error error("Invalid attachment parameters");
        lcf_log_error(error.what());
        throw error;
    }
}

VulkanAttachment::~VulkanAttachment() noexcept = default;

void VulkanAttachment::blitTo(VulkanCommandBufferObject & cmd, VulkanAttachment &dst, vk::Filter filter, const Offset3DPair &src_offsets, const Offset3DPair &dst_offsets)
{
    m_proxy_sp->blitTo(cmd,
        vk::ImageSubresourceLayers(m_proxy_sp->getAspectFlags(), m_mip_level, m_layer, m_layer_count),
        src_offsets,
        dst.getImageProxy(),
        vk::ImageSubresourceLayers(dst.getImageProxy().getAspectFlags(), dst.getMipLevel(), dst.getLayer(), dst.getLayerCount()),
        dst_offsets,
        filter
    );
}

bool VulkanAttachment::isValid() const noexcept
{
    return m_proxy_sp and m_proxy_sp->isCreated();
}

void VulkanAttachment::blitTo(VulkanCommandBufferObject &cmd, VulkanAttachment &dst, vk::Filter filter)
{
    const auto & [sx, sy, sz] = this->getExtent();
    const auto & [dx, dy, dz] = dst.getExtent();
    this->blitTo(cmd, dst, filter,
        {{ 0, 0, 0 }, {static_cast<int32_t>(sx), static_cast<int32_t>(sy), static_cast<int32_t>(sz)}},
        {{ 0, 0, 0 }, {static_cast<int32_t>(dx), static_cast<int32_t>(dy), static_cast<int32_t>(dz)}}
    );
}

void VulkanAttachment::copyTo(VulkanCommandBufferObject & cmd, VulkanAttachment &dst, const vk::Offset3D &src_offset, const vk::Offset3D &dst_offset, const vk::Extent3D &extent)
{
    m_proxy_sp->copyTo(cmd,
        vk::ImageSubresourceLayers(m_proxy_sp->getAspectFlags(), m_mip_level, m_layer, m_layer_count),
        src_offset,
        dst.getImageProxy(),
        vk::ImageSubresourceLayers(dst.getImageProxy().getAspectFlags(), dst.getMipLevel(), dst.getLayer(), dst.getLayerCount()),
        dst_offset,
        extent
    );
}

vk::ImageSubresourceRange VulkanAttachment::getSubresourceRange() const noexcept
{
    return vk::ImageSubresourceRange(m_proxy_sp->getAspectFlags(), m_mip_level, 1, m_layer, m_layer_count);
}

vk::ImageView VulkanAttachment::getImageView() const noexcept
{
    return m_proxy_sp->getView(this->getSubresourceRange());
}

void VulkanAttachment::transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout)
{
    m_proxy_sp->transitLayout(cmd, this->getSubresourceRange(), new_layout);
}

vk::Extent3D VulkanAttachment::getExtent() const noexcept
{
    vk::Extent3D extent = m_proxy_sp->getExtent();
    return { extent.width >> m_mip_level, extent.height >> m_mip_level, extent.depth };
}

std::optional<vk::ImageLayout> VulkanAttachment::getLayout() const noexcept
{
    return m_proxy_sp->getLayout(m_layer, m_layer_count, m_mip_level, 1);
}
