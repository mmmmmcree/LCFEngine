#include "vk_core/descriptor_set/heap/DescriptorHeap.h"
#include "vk_core/descriptor_set/heap/DescriptorHeapAllocator.h"
#include "vk_core/descriptor_set/heap/DescriptorHeapLayout.h"
#include "vk_core/utils/align.h"
#include "vk_core/error.h"
#include "vk_core/utils/enum/vk_enums_traits.h"
#include <cassert>

namespace lcf::vkc::dsh {

std::error_code DescriptorHeap::create(DescriptorHeapAllocator & allocator, const DescriptorHeapLayout & layout) noexcept
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

DescriptorHeap::Self & DescriptorHeap::setBuffer(uint32_t index, vk::DescriptorType type, const vkc::BufferView & buffer_view) noexcept
{
    assert(enum_traits<vk::DescriptorType>::is_buffer_descriptor(type) and index < m_layout.getBufferCount());
    m_authority_buffers.insert_or_assign(index, AuthorityBufferBinding {type, buffer_view});
    ++m_authority_resource_version;
    return *this;
}

DescriptorHeap::Self & DescriptorHeap::setImage(uint32_t index, vk::DescriptorType type, vk::ImageLayout image_layout, const vkc::ImageView & image_view) noexcept
{
    assert(enum_traits<vk::DescriptorType>::is_image_descriptor(type) and index < m_layout.getImageCount());
    m_authority_images.insert_or_assign(index, AuthorityImageBinding {type, image_layout, image_view});
    ++m_authority_resource_version;
    return *this;
}

DescriptorHeap::Self & DescriptorHeap::setSampler(uint32_t index, const vkc::Sampler & sampler) noexcept
{
    assert(index < m_layout.getSamplerCount());
    m_autority_samplers.insert_or_assign(index, AuthoritySamplerBinding {sampler});
    ++m_authority_sampler_version;
    return *this;
}

} // namespace lcf::vkc::dsh
