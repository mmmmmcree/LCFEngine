#include "Vulkan/VulkanImageObject.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/vulkan_memory_resources.h"
#include "Vulkan/VulkanBufferObject.h"
#include "log.h"

using namespace lcf::render;

VulkanImageObject::VulkanImageObject() :
    m_proxy_sp(std::make_shared<VulkanImageProxy>())
{
}

VulkanImageObject::~VulkanImageObject()
{
}

bool VulkanImageObject::create(VulkanContext * context_p)
{
    return m_proxy_sp->_create(context_p, vk::ImageTiling::eOptimal, MemoryAllocationCreateInfo {vk::MemoryPropertyFlagBits::eDeviceLocal});
}

bool VulkanImageObject::create(VulkanContext *context_p, vk::Image external_image)
{
    m_proxy_sp->m_context_p = context_p;
    m_proxy_sp->m_image_rp = make_resource_ptr<VulkanImage>(external_image);
    auto interval = VulkanImageProxy::LayoutMapInterval::right_open(0, m_proxy_sp->getMipLevelCount() * m_proxy_sp->m_array_layers);
    m_proxy_sp->m_layout_map.set(std::make_pair(interval, vk::ImageLayout::eUndefined));
    return this->isCreated();
}

void VulkanImageObject::setData(VulkanCommandBufferObject &cmd, std::span<const std::byte> data, uint32_t layer)
{
    cmd.acquireResourceLease(m_proxy_sp->lease());
    VulkanBufferProxy staging_buffer;
    staging_buffer.setUsage(GPUBufferUsage::eStaging)
        .create(m_proxy_sp->m_context_p, data.size_bytes());
    staging_buffer.writeSegmentDirectly(data);
    cmd.acquireResourceLease(staging_buffer.lease());
    vk::BufferImageCopy region;
    region.setImageSubresource({ m_proxy_sp->getAspectFlags(), 0, 0, 1 })
        .setImageOffset({ 0, 0, 0 })
        .setImageExtent(m_proxy_sp->getExtent());
    m_proxy_sp->transitLayout(cmd, vk::ImageLayout::eTransferDstOptimal);
    cmd.copyBufferToImage(staging_buffer.getHandle(), m_proxy_sp->getHandle(), vk::ImageLayout::eTransferDstOptimal, region);
    m_proxy_sp->transitLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void VulkanImageObject::generateMipmaps(VulkanCommandBufferObject & cmd)
{
    cmd.acquireResourceLease(m_proxy_sp->lease());
    using Offset3DPair = std::pair<vk::Offset3D, vk::Offset3D>;
    auto extent = m_proxy_sp->getExtent();
    Offset3DPair src_offsets = {{0, 0, 0}, {static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1}};
    auto aspect_flags = m_proxy_sp->getAspectFlags();
    auto array_layers = m_proxy_sp->m_array_layers;
    for (uint32_t level = 1; level < m_proxy_sp->getMipLevelCount(); ++level) {
        Offset3DPair dst_offsets = {{0, 0, 0}, {static_cast<int32_t>(extent.width) >> level, static_cast<int32_t>(extent.height) >> level, 1}};
        m_proxy_sp->blitTo(cmd, {aspect_flags, level - 1, 0, array_layers}, src_offsets,
            *m_proxy_sp, {aspect_flags, level, 0, array_layers}, dst_offsets, vk::Filter::eLinear);
        src_offsets = dst_offsets;
    }
    m_proxy_sp->transitLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
}
