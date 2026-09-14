#include "vk_core/descriptor_set/heap/DescriptorHeap.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/descriptor_set/heap/DescriptorHeapAllocator.h"
#include "vk_core/descriptor_set/heap/DescriptorHeapLayout.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/utils/align.h"
#include "vk_core/error.h"
#include "vk_core/utils/enum/vk_enums_traits.h"
#include <array>
#include <cassert>
#include <vector>

namespace lcf::vkc::dsh {

std::error_code DescriptorHeapProxy::create(DescriptorHeapAllocator & allocator, const DescriptorHeapLayout & layout) noexcept
{
    if (m_allocator_p) { return make_error_code(errc::already_created); }
    const auto & dsh_props = allocator.getDescriptorHeapProperties();
    vk::DeviceSize buffer_descriptor_stride = vkc::utils::align_up(dsh_props.bufferDescriptorSize, dsh_props.bufferDescriptorAlignment);
    vk::DeviceSize image_descriptor_stride = vkc::utils::align_up(dsh_props.imageDescriptorSize, dsh_props.imageDescriptorAlignment);
    vk::DeviceSize sampler_descriptor_stride = vkc::utils::align_up(dsh_props.samplerDescriptorSize, dsh_props.samplerDescriptorAlignment);
    DescriptorHeapAllocateInfo info;
    info.m_resource_heap_size = layout.getBufferCount() * buffer_descriptor_stride + layout.getImageCount() * image_descriptor_stride;
    info.m_sampler_heap_size = layout.getSamplerCount() * sampler_descriptor_stride;
    auto expected_allocation = allocator.allocate(info);
    if (not expected_allocation) { return expected_allocation.error(); }
    m_allocator_p = &allocator;
    m_layout = layout;
    const auto & allocation = expected_allocation.value();
    m_resource_buffer = std::move(allocation.m_resource_buffer);
    m_sampler_buffer = std::move(allocation.m_sampler_buffer);
    return {};
}

auto DescriptorHeapProxy::setBuffer(uint32_t index, vk::DescriptorType type, const vkc::BufferView & buffer_view) noexcept -> Self &
{
    assert(enum_traits<vk::DescriptorType>::is_buffer_descriptor(type) and index < m_layout.getBufferCount());
    m_authority_buffers.insert_or_assign(index, AuthorityBufferBinding {type, buffer_view});
    ++m_authority_resource_version;
    return *this;
}

auto DescriptorHeapProxy::setImage(uint32_t index, vk::DescriptorType type, vk::ImageLayout image_layout, const vkc::ImageView & image_view) noexcept -> Self &
{
    assert(enum_traits<vk::DescriptorType>::is_image_descriptor(type) and index < m_layout.getImageCount());
    m_authority_images.insert_or_assign(index, AuthorityImageBinding {type, image_layout, image_view});
    ++m_authority_resource_version;
    return *this;
}

auto DescriptorHeapProxy::setSampler(uint32_t index, const vkc::Sampler & sampler) noexcept -> Self &
{
    assert(index < m_layout.getSamplerCount());
    m_autority_samplers.insert_or_assign(index, AuthoritySamplerBinding {sampler});
    ++m_authority_sampler_version;
    return *this;
}

std::error_code DescriptorHeapProxy::updateIfDirty(CommandBufferProxy & cmd) noexcept
{
    if (not m_allocator_p) { return std::make_error_code(std::errc::invalid_argument); }
    if (m_uploaded_resource_version == m_authority_resource_version and
        m_uploaded_sampler_version == m_authority_sampler_version) {
        return {};
    }
    const auto & heap_allocator = *m_allocator_p;
    const auto & props = heap_allocator.getDescriptorHeapProperties();
    const auto buffer_stride = utils::align_up(props.bufferDescriptorSize, props.bufferDescriptorAlignment);
    const auto image_stride = utils::align_up(props.imageDescriptorSize, props.imageDescriptorAlignment);
    const auto sampler_stride = utils::align_up(props.samplerDescriptorSize, props.samplerDescriptorAlignment);
    const auto resource_data_size = m_layout.getBufferCount() * buffer_stride + m_layout.getImageCount() * image_stride;
    const auto sampler_data_size = m_layout.getSamplerCount() * sampler_stride;
    const auto sampler_staging_offset = resource_data_size;
    vk::BufferCreateInfo buffer_info;
    buffer_info.setSize(resource_data_size + sampler_data_size)
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);
    MemoryAllocationInfo alloc_info;
    alloc_info.setAccess(MemoryAccess::eHostSequentialWrite);
    vkc::Buffer staging_buffer;
    if (auto ec = staging_buffer.create(heap_allocator.getMemoryAllocator(), buffer_info, alloc_info)) { return ec; }
    auto mapped = staging_buffer.getMappedMemorySpan();
    auto device = heap_allocator.getMemoryAllocator().getDevice();
    std::vector<vk::ResourceDescriptorInfoEXT> resource_infos;
    std::vector<vk::HostAddressRangeEXT> resource_ranges;
    std::vector<vk::DeviceAddressRangeEXT> address_ranges;
    std::vector<vk::ImageViewCreateInfo> view_infos;
    std::vector<vk::ImageDescriptorInfoEXT> image_infos;
    resource_infos.reserve(m_authority_buffers.size() + m_authority_images.size());
    resource_ranges.reserve(resource_infos.capacity());
    address_ranges.reserve(m_authority_buffers.size());
    view_infos.reserve(m_authority_images.size());
    image_infos.reserve(m_authority_images.size());
    for (const auto & [index, binding] : m_authority_buffers) {
        address_ranges.emplace_back(binding.m_buffer_view.getDeviceAddress(), binding.m_buffer_view.getSizeInBytes());
        vk::ResourceDescriptorDataEXT data;
        data.setPAddressRange(&address_ranges.back());
        resource_infos.emplace_back(binding.m_type, data);
        resource_ranges.emplace_back(mapped.data() + index * buffer_stride, props.bufferDescriptorSize);
    }
    const auto image_base = m_layout.getBufferCount() * buffer_stride;
    for (const auto & [index, binding] : m_authority_images) {
        view_infos.emplace_back(binding.m_image_view.makeInfo());
        image_infos.emplace_back(&view_infos.back(), binding.m_layout);
        vk::ResourceDescriptorDataEXT data;
        data.setPImage(&image_infos.back());
        resource_infos.emplace_back(binding.m_type, data);
        resource_ranges.emplace_back(mapped.data() + image_base + index * image_stride, props.imageDescriptorSize);
    }
    if (not resource_infos.empty()) {
        const auto result = device.writeResourceDescriptorsEXT(
            static_cast<uint32_t>(resource_infos.size()), resource_infos.data(), resource_ranges.data());
        if (result != vk::Result::eSuccess) { return vk::make_error_code(result); }
    }
    std::vector<vk::SamplerCreateInfo> sampler_infos;
    std::vector<vk::HostAddressRangeEXT> sampler_ranges;
    sampler_infos.reserve(m_autority_samplers.size());
    sampler_ranges.reserve(m_autority_samplers.size());
    for (const auto & [index, binding] : m_autority_samplers) {
        sampler_infos.emplace_back(binding.m_sampler.getInfo());
        sampler_ranges.emplace_back(mapped.data() + sampler_staging_offset + index * sampler_stride, props.samplerDescriptorSize);
    }
    if (not sampler_infos.empty()) {
        const auto result = device.writeSamplerDescriptorsEXT(
            static_cast<uint32_t>(sampler_infos.size()), sampler_infos.data(), sampler_ranges.data());
        if (result != vk::Result::eSuccess) { return vk::make_error_code(result); }
    }
    if (auto ec = staging_buffer.flush()) { return ec; }
    std::array<vk::BufferMemoryBarrier2, 2> to_transfer_write;
    to_transfer_write[0].setSrcStageMask(vk::PipelineStageFlagBits2::eAllCommands)
        .setSrcAccessMask(vk::AccessFlagBits2::eResourceHeapReadEXT)
        .setDstStageMask(vk::PipelineStageFlagBits2::eCopy)
        .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setBuffer(m_resource_buffer.handle())
        .setSize(resource_data_size);
    to_transfer_write[1] = to_transfer_write[0]
        .setSrcAccessMask(vk::AccessFlagBits2::eSamplerHeapReadEXT)
        .setBuffer(m_sampler_buffer.handle())
        .setSize(sampler_data_size);
    vk::DependencyInfo before_copy;
    before_copy.setBufferMemoryBarriers(to_transfer_write);
    cmd.pipelineBarrier2(before_copy);
    cmd.copyBuffer(staging_buffer.handle(), m_resource_buffer.handle(), vk::BufferCopy {0u, 0u, resource_data_size});
    cmd.copyBuffer(staging_buffer.handle(), m_sampler_buffer.handle(), vk::BufferCopy {sampler_staging_offset, 0u, sampler_data_size});
    std::array<vk::BufferMemoryBarrier2, 2> to_heap_read;
    to_heap_read[0] = to_transfer_write[0]
        .setSrcStageMask(vk::PipelineStageFlagBits2::eCopy)
        .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eAllCommands)
        .setDstAccessMask(vk::AccessFlagBits2::eResourceHeapReadEXT);
    to_heap_read[1] = to_heap_read[0]
        .setDstAccessMask(vk::AccessFlagBits2::eSamplerHeapReadEXT)
        .setBuffer(m_sampler_buffer.handle());
    vk::DependencyInfo after_copy;
    after_copy.setBufferMemoryBarriers(to_heap_read);
    cmd.pipelineBarrier2(after_copy);
    cmd.pinLease(staging_buffer.lease());
    m_uploaded_resource_version = m_authority_resource_version;
    m_uploaded_sampler_version = m_authority_sampler_version;
    return {};
}

void DescriptorHeapProxy::bind(CommandBufferProxy & cmd) const noexcept
{
    const auto & props = m_allocator_p->getDescriptorHeapProperties();
    vk::BindHeapInfoEXT resource_info;
    resource_info.setHeapRange({m_resource_buffer.getDeviceAddress(), m_resource_buffer.getSizeInBytes()})
        .setReservedRangeOffset(m_resource_buffer.getSizeInBytes() - props.minResourceHeapReservedRange)
        .setReservedRangeSize(props.minResourceHeapReservedRange);
    vk::BindHeapInfoEXT sampler_info;
    sampler_info.setHeapRange({m_sampler_buffer.getDeviceAddress(), m_sampler_buffer.getSizeInBytes()})
        .setReservedRangeOffset(m_sampler_buffer.getSizeInBytes() - props.minSamplerHeapReservedRange)
        .setReservedRangeSize(props.minSamplerHeapReservedRange);
    cmd.bindResourceHeapEXT(resource_info);
    cmd.bindSamplerHeapEXT(sampler_info);
    cmd.pinLease(m_resource_buffer.lease()).pinLease(m_sampler_buffer.lease());
}

} // namespace lcf::vkc::dsh
