#include "Vulkan/VulkanBufferProxy.h"
#include "Vulkan/vulkan_memory_resources.h"
#include "Vulkan/VulkanContext.h"
#include <boost/align.hpp>

using namespace lcf::render;

VulkanBufferProxy::~VulkanBufferProxy() = default;

bool VulkanBufferProxy::create(VulkanContext *context_p, uint64_t size_in_bytes)
{
    m_context_p = context_p;
    vk::BufferUsageFlags usage_flags;
    vk::MemoryPropertyFlags memory_flags;
    switch (this->getUsage()) {
        case GPUBufferUsage::eVertex : {
            usage_flags = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        } break;
        case GPUBufferUsage::eIndex : {
            usage_flags = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        } break;
        case GPUBufferUsage::eUniform : {
            usage_flags = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
        } break;
        case GPUBufferUsage::eShaderStorage : {
            usage_flags = vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eTransferDst |
                vk::BufferUsageFlagBits::eShaderDeviceAddress;
        } break;
        case GPUBufferUsage::eIndirect : {
            usage_flags = vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eTransferSrc |
                vk::BufferUsageFlagBits::eTransferDst |
                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                vk::BufferUsageFlagBits::eIndirectBuffer;
        } break;
        case GPUBufferUsage::eStaging : {
            usage_flags = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
        } break;
    }
    switch (this->getPattern()) {
        case GPUBufferPattern::eDynamic : {
            memory_flags = vk::MemoryPropertyFlagBits::eHostVisible;
        } break;
        case GPUBufferPattern::eStatic : {
            memory_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
        } break;
    }
    size_in_bytes = boost::alignment::align_up(size_in_bytes, 4u);
    vk::BufferCreateInfo buffer_info {{}, size_in_bytes, usage_flags, vk::SharingMode::eExclusive};
    MemoryAllocationCreateInfo memory_info {memory_flags};
    auto device = m_context_p->getDevice();
    auto & memory_allocator = m_context_p->getMemoryAllocator();
    m_buffer_sp = memory_allocator.createBuffer(buffer_info, memory_info);
    if (not m_buffer_sp) { return false; }
    if (buffer_info.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        m_device_address = device.getBufferAddress(m_buffer_sp->getHandle());
    }
    return true;
}

bool VulkanBufferProxy::recreate(uint64_t size_in_bytes)
{
    return this->create(m_context_p, size_in_bytes);
}

void VulkanBufferProxy::writeSegmentDirectly(
    const BufferWriteSegment & segment,
    uint64_t dst_offset_in_bytes) noexcept
{
    memcpy(this->getMappedMemoryPtr() + segment.getBeginOffsetInBytes() + dst_offset_in_bytes,
        segment.getData(), segment.getSizeInBytes());
    m_buffer_sp->flush(segment.getBeginOffsetInBytes() + dst_offset_in_bytes, segment.getSizeInBytes());
}

void VulkanBufferProxy::writeSegmentsDirectly(
    const BufferWriteSegments & segments,
    uint64_t dst_offset_in_bytes) noexcept
{
    for (const auto & write_segment : segments) {
        memcpy(this->getMappedMemoryPtr() + write_segment.getBeginOffsetInBytes() + dst_offset_in_bytes,
            write_segment.getData(), write_segment.getSizeInBytes());
    }
    m_buffer_sp->flush(segments.getLowerBoundInBytes() + dst_offset_in_bytes, segments.getValidSizeInBytes());
}

uint64_t VulkanBufferProxy::getSizeInBytes() const noexcept
{
    return m_buffer_sp->getSize();
}

std::byte *VulkanBufferProxy::getMappedMemoryPtr() const noexcept
{
    return m_buffer_sp->getMappedMemoryPtr();
}

vk::Buffer VulkanBufferProxy::getHandle() const noexcept
{
    return m_buffer_sp->getHandle();
}

vk::DescriptorBufferInfo VulkanBufferProxy::generateBufferInfo() const noexcept
{
    return {this->getHandle(), 0u, vk::WholeSize};
}