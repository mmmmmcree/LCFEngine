#include "Vulkan/memory/details/VulkanBufferProxy.h"
#include "Vulkan/memory/vulkan_memory_resources.h"
#include "Vulkan/VulkanContext.h"
#include <boost/align.hpp>

using namespace lcf::render;
namespace stdr = std::ranges;

VulkanBufferProxy::~VulkanBufferProxy() = default;

bool VulkanBufferProxy::create(VulkanContext *context_p, uint64_t size_in_bytes)
{
    m_context_p = context_p;
    vk::BufferUsageFlags2 usage_flags {};
    vk::MemoryPropertyFlags memory_flags;
    switch (this->getUsage()) {
        case GPUBufferUsage::eVertex : {
            usage_flags = vk::BufferUsageFlagBits2::eVertexBuffer | vk::BufferUsageFlagBits2::eTransferDst;
        } break;
        case GPUBufferUsage::eIndex : {
            usage_flags = vk::BufferUsageFlagBits2::eIndexBuffer | vk::BufferUsageFlagBits2::eTransferDst;
        } break;
        case GPUBufferUsage::eUniform : {
            usage_flags = vk::BufferUsageFlagBits2::eUniformBuffer | vk::BufferUsageFlagBits2::eTransferDst;
        } break;
        case GPUBufferUsage::eShaderStorage : {
            usage_flags = vk::BufferUsageFlagBits2::eStorageBuffer |
                vk::BufferUsageFlagBits2::eTransferDst |
                vk::BufferUsageFlagBits2::eShaderDeviceAddress;
        } break;
        case GPUBufferUsage::eIndirect : {
            usage_flags = vk::BufferUsageFlagBits2::eStorageBuffer |
                vk::BufferUsageFlagBits2::eTransferSrc |
                vk::BufferUsageFlagBits2::eTransferDst |
                vk::BufferUsageFlagBits2::eShaderDeviceAddress |
                vk::BufferUsageFlagBits2::eIndirectBuffer;
        } break;
        case GPUBufferUsage::eStaging : {
            usage_flags = vk::BufferUsageFlagBits2::eTransferSrc | vk::BufferUsageFlagBits2::eTransferDst;
        } break;
        case GPUBufferUsage::ePreprocess : {
            usage_flags = vk::BufferUsageFlagBits2::ePreprocessBufferEXT |
                vk::BufferUsageFlagBits2::eIndirectBuffer |
                vk::BufferUsageFlagBits2::eShaderDeviceAddress;
        } break;
        default: break;
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
    vk::BufferUsageFlags2CreateInfo usage_flags_2_info;
    usage_flags_2_info.setUsage(usage_flags);
    vk::BufferCreateInfo buffer_info {};
    buffer_info.setSize(size_in_bytes)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setPNext(&usage_flags_2_info);
    MemoryAllocationCreateInfo memory_info {memory_flags};
    auto device = m_context_p->getDevice();
    auto & memory_allocator = m_context_p->getMemoryAllocator();
    m_buffer_rp = memory_allocator.createBuffer(buffer_info, memory_info);
    if (not m_buffer_rp) { return false; }
    if (usage_flags & vk::BufferUsageFlagBits2::eShaderDeviceAddress) {
        m_device_address = device.getBufferAddress(m_buffer_rp->getHandle());
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
    auto dst = this->getMappedMemorySpan().subspan(segment.getBeginOffsetInBytes() + dst_offset_in_bytes);
    stdr::copy(segment.getDataSpan(), dst.begin());
    m_buffer_rp->flush(segment.getBeginOffsetInBytes() + dst_offset_in_bytes, segment.getSizeInBytes());
}

void VulkanBufferProxy::writeSegmentsDirectly(
    const BufferWriteSegments & segments,
    uint64_t dst_offset_in_bytes) noexcept
{
    for (const auto & write_segment : segments) {
        auto dst = this->getMappedMemorySpan().subspan(write_segment.getBeginOffsetInBytes() + dst_offset_in_bytes);
        stdr::copy(write_segment.getDataSpan(), dst.begin());
    }
    m_buffer_rp->flush(segments.getLowerBoundInBytes() + dst_offset_in_bytes, segments.getValidSizeInBytes());
}

uint64_t VulkanBufferProxy::getSizeInBytes() const noexcept
{
    return m_buffer_rp->getSize();
}

std::span<std::byte> VulkanBufferProxy::getMappedMemorySpan() const noexcept
{
    return m_buffer_rp->getMappedMemorySpan();
}

vk::Buffer VulkanBufferProxy::getHandle() const noexcept
{
    return m_buffer_rp->getHandle();
}

vk::DescriptorBufferInfo VulkanBufferProxy::generateBufferInfo(vk::DeviceSize offset, vk::DeviceSize range) const noexcept
{
    return {this->getHandle(), offset, range};
}