#include "Vulkan/memory/VulkanImageObject.h"
#include "Vulkan/memory/details/VulkanImageProxy.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/memory/vulkan_memory_resources.h"
#include "Vulkan/memory/VulkanBufferObject.h"
#include "log.h"

using namespace lcf;
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

VulkanImageObject & VulkanImageObject::addImageFlags(vk::ImageCreateFlags flags) noexcept
{
    m_proxy_sp->addImageFlags(flags);
    return *this;
}

VulkanImageObject & VulkanImageObject::setFormat(vk::Format format) noexcept
{
    m_proxy_sp->setFormat(format);
    return *this;
}

VulkanImageObject & VulkanImageObject::setExtent(vk::Extent3D extent) noexcept
{
    m_proxy_sp->setExtent(extent);
    return *this;
}

VulkanImageObject & VulkanImageObject::setMipmapped(bool mipmapped) noexcept
{
    m_proxy_sp->setMipmapped(mipmapped);
    return *this;
}

VulkanImageObject & VulkanImageObject::setArrayLayers(uint16_t array_layers) noexcept
{
    m_proxy_sp->setArrayLayers(array_layers);
    return *this;
}

VulkanImageObject & VulkanImageObject::setSamples(vk::SampleCountFlagBits samples) noexcept
{
    m_proxy_sp->setSamples(samples);
    return *this;
}

VulkanImageObject & VulkanImageObject::setUsage(vk::ImageUsageFlags usage) noexcept
{
    m_proxy_sp->setUsage(usage);
    return *this;
}

bool VulkanImageObject::isCreated() const noexcept
{
    return m_proxy_sp and m_proxy_sp->isCreated(); 
}

ResourceLease VulkanImageObject::lease() const noexcept
{
    return m_proxy_sp->lease();
}

vk::Image VulkanImageObject::getHandle() const noexcept
{
    return m_proxy_sp->getHandle();
}

std::span<std::byte> VulkanImageObject::getMappedMemorySpan() const noexcept
{
    return m_proxy_sp->getMappedMemorySpan();
}

vk::ImageView VulkanImageObject::getDefaultView() const
{
    return m_proxy_sp->getDefaultView();
}

vk::ImageView VulkanImageObject::getView(const vk::ImageSubresourceRange & subresource_range) const
{
    return m_proxy_sp->getView(subresource_range);
}

vk::ImageCreateFlags VulkanImageObject::getImageFlags() const
{
    return m_proxy_sp->getImageFlags();
}

vk::ImageType VulkanImageObject::getImageType() const noexcept
{
    return m_proxy_sp->getImageType();
}

vk::Format VulkanImageObject::getFormat() const
{
    return m_proxy_sp->getFormat();
}

vk::Extent3D VulkanImageObject::getExtent() const
{
    return m_proxy_sp->getExtent();
}

uint32_t VulkanImageObject::getMipLevelCount() const noexcept
{
    return m_proxy_sp->getMipLevelCount();
}

uint32_t VulkanImageObject::getArrayLayerCount() const
{
    return m_proxy_sp->getArrayLayerCount();
}

vk::SampleCountFlagBits VulkanImageObject::getSamples() const
{
    return m_proxy_sp->getSamples();
}

vk::ImageAspectFlags VulkanImageObject::getAspectFlags() const noexcept
{
    return m_proxy_sp->getAspectFlags();
}

std::optional<vk::ImageLayout> VulkanImageObject::getLayout() const noexcept
{
    return m_proxy_sp->getLayout();
}

void VulkanImageObject::transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout)
{
    m_proxy_sp->transitLayout(cmd, new_layout);
}

void VulkanImageObject::transitLayout(VulkanCommandBufferObject & cmd, const vk::ImageSubresourceRange & subresource_range, vk::ImageLayout new_layout)
{
    m_proxy_sp->transitLayout(cmd, subresource_range, new_layout);
}

void VulkanImageObject::copyFrom(VulkanCommandBufferObject & cmd, vk::Buffer buffer, std::span<const vk::BufferImageCopy> regions)
{
    m_proxy_sp->copyFrom(cmd, buffer, regions);
}