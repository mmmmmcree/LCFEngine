#include "vk_core/descriptor_set/buffer/DescriptorSetProxy.h"
#include "vk_core/descriptor_set/buffer/DescriptorSetAllocator.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/memory/Image.h"
#include "vk_core/sampler/Sampler.h"
#include "vk_core/utils/enum/vk_enums_traits.h"
#include "vk_core/error.h"
#include <cassert>
#include <cstring>

namespace lcf::vkc::dsb {

DescriptorSetProxy::~DescriptorSetProxy() noexcept = default;

std::error_code DescriptorSetProxy::create(DescriptorSetAllocator & allocator, const DescriptorSetLayout & layout) noexcept
{
    if (m_allocator_p) { return make_error_code(errc::already_created); }
    if (not layout.handle()) { return std::make_error_code(std::errc::invalid_argument); }
    auto allocation_result = allocator.allocate(layout);
    if (not allocation_result) { return allocation_result.error(); }
    m_allocator_p = &allocator;
    m_layout = layout;
    m_descriptor_set_buffer = std::move(allocation_result->m_buffer);
    m_authority_bindings.reserve(layout.getBindings().size());
    for (const auto & binding : m_layout.getBindings()) {
        auto & bindings = m_authority_bindings.emplace_back();
        bindings.resize(binding.descriptorCount);
    }
    return {};
}

auto DescriptorSetProxy::setBuffer(
    uint32_t binding,
    uint32_t array_index,
    const vkc::Buffer & buffer,
    vk::DeviceSize offset,
    vk::DeviceSize range) noexcept -> Self &
{
    const auto & layout_binding = m_layout.getBindings().at(binding);
    assert(binding == layout_binding.binding and enum_traits<vk::DescriptorType>::is_buffer_descriptor(layout_binding.descriptorType));
    if (range == vk::WholeSize) { range = buffer.getSizeInBytes() - offset; }
    auto & authority_binding = m_authority_bindings[binding][array_index];
    authority_binding.m_descriptor_info = vk::DescriptorAddressInfoEXT {buffer.getDeviceAddress() + offset, range};
    authority_binding.m_resource_leases[0] = buffer.lease();
    ++m_authority_version;
    return *this;
}

auto DescriptorSetProxy::setBuffer(
    uint32_t binding,
    const vkc::Buffer & buffer,
    vk::DeviceSize offset,
    vk::DeviceSize range) noexcept -> Self &
{
    return this->setBuffer(binding, 0u, buffer, offset, range);
}

auto DescriptorSetProxy::setImage(
    uint32_t binding,
    uint32_t array_index,
    const vkc::ImageView & image_view,
    vk::ImageLayout image_layout) noexcept -> Self &
{
    const auto & layout_binding = m_layout.getBindings().at(binding);
    assert(binding == layout_binding.binding and enum_traits<vk::DescriptorType>::is_image_descriptor(layout_binding.descriptorType));
    auto & authority_binding = m_authority_bindings[binding][array_index];
    vk::DescriptorImageInfo image_info;
    if (const auto * current_info_p = std::get_if<vk::DescriptorImageInfo>(&authority_binding.m_descriptor_info)) {
        image_info = *current_info_p;
    }
    image_info.setImageView(image_view.handle()).setImageLayout(image_layout);
    authority_binding.m_descriptor_info = image_info;
    authority_binding.m_resource_leases[0] = image_view.lease();
    ++m_authority_version;
    return *this;
}

auto DescriptorSetProxy::setImage(
    uint32_t binding,
    const vkc::ImageView & image_view,
    vk::ImageLayout image_layout) noexcept -> Self &
{
    return this->setImage(binding, 0u, image_view, image_layout);
}

auto DescriptorSetProxy::setSampler(uint32_t binding, uint32_t array_index, const vkc::Sampler & sampler) noexcept -> Self &
{
    const auto & layout_binding = m_layout.getBindings().at(binding);
    assert(binding == layout_binding.binding and enum_traits<vk::DescriptorType>::is_sampler_descriptor(layout_binding.descriptorType));
    auto & authority_binding = m_authority_bindings[binding][array_index];
    vk::DescriptorImageInfo image_info;
    if (const auto * current_info_p = std::get_if<vk::DescriptorImageInfo>(&authority_binding.m_descriptor_info)) {
        image_info = *current_info_p;
    }
    image_info.setSampler(sampler.handle());
    authority_binding.m_descriptor_info = image_info;
    authority_binding.m_resource_leases[1] = sampler.lease();
    ++m_authority_version;
    return *this;
}

auto DescriptorSetProxy::setSampler(uint32_t binding, const vkc::Sampler & sampler) noexcept -> Self &
{
    return this->setSampler(binding, 0u, sampler);
}

namespace {

std::size_t get_descriptor_size(
    vk::DescriptorType descriptor_type,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT & properties) noexcept
{
    switch (descriptor_type) {
        case vk::DescriptorType::eSampler: return properties.samplerDescriptorSize;
        case vk::DescriptorType::eCombinedImageSampler: return properties.combinedImageSamplerDescriptorSize;
        case vk::DescriptorType::eSampledImage: return properties.sampledImageDescriptorSize;
        case vk::DescriptorType::eStorageImage: return properties.storageImageDescriptorSize;
        case vk::DescriptorType::eUniformBuffer: return properties.uniformBufferDescriptorSize;
        case vk::DescriptorType::eStorageBuffer: return properties.storageBufferDescriptorSize;
        case vk::DescriptorType::eInputAttachment: return properties.inputAttachmentDescriptorSize;
        default: return 0u;
    }
}

vk::DescriptorGetInfoEXT make_descriptor_get_info(vk::DescriptorType descriptor_type, const auto & descriptor_info) noexcept
{
    vk::DescriptorDataEXT descriptor_data;
    switch (descriptor_type) {
        case vk::DescriptorType::eSampler:
            descriptor_data.setPSampler(&std::get<vk::DescriptorImageInfo>(descriptor_info).sampler);
            break;
        case vk::DescriptorType::eCombinedImageSampler:
            descriptor_data.setPCombinedImageSampler(&std::get<vk::DescriptorImageInfo>(descriptor_info));
            break;
        case vk::DescriptorType::eSampledImage:
            descriptor_data.setPSampledImage(&std::get<vk::DescriptorImageInfo>(descriptor_info));
            break;
        case vk::DescriptorType::eStorageImage:
            descriptor_data.setPStorageImage(&std::get<vk::DescriptorImageInfo>(descriptor_info));
            break;
        case vk::DescriptorType::eUniformBuffer:
            descriptor_data.setPUniformBuffer(&std::get<vk::DescriptorAddressInfoEXT>(descriptor_info));
            break;
        case vk::DescriptorType::eStorageBuffer:
            descriptor_data.setPStorageBuffer(&std::get<vk::DescriptorAddressInfoEXT>(descriptor_info));
            break;
        case vk::DescriptorType::eInputAttachment:
            descriptor_data.setPInputAttachmentImage(&std::get<vk::DescriptorImageInfo>(descriptor_info));
            break;
        default:
            break;
    }
    return vk::DescriptorGetInfoEXT {descriptor_type, descriptor_data};
}

void write_combined_image_sampler_array(
    vk::Device device,
    const auto & authority_bindings,
    std::span<std::byte> mapped_memory,
    vk::DeviceSize binding_offset,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT & properties) noexcept
{
    std::vector<std::byte> descriptor_data(properties.combinedImageSamplerDescriptorSize);
    const vk::DeviceSize sampler_array_offset = binding_offset + authority_bindings.size() * properties.sampledImageDescriptorSize;
    for (uint32_t array_index = 0u; array_index < authority_bindings.size(); ++array_index) {
        auto descriptor_get_info = make_descriptor_get_info(
            vk::DescriptorType::eCombinedImageSampler,
            authority_bindings[array_index].m_descriptor_info);
        device.getDescriptorEXT(descriptor_get_info, descriptor_data.size(), descriptor_data.data());
        std::memcpy(
            mapped_memory.data() + binding_offset + array_index * properties.sampledImageDescriptorSize,
            descriptor_data.data(),
            properties.sampledImageDescriptorSize);
        std::memcpy(
            mapped_memory.data() + sampler_array_offset + array_index * properties.samplerDescriptorSize,
            descriptor_data.data() + properties.sampledImageDescriptorSize,
            properties.samplerDescriptorSize);
    }
}

void write_descriptors(
    vk::Device device,
    const auto & layout_bindings,
    const auto & layout_offsets,
    const auto & authority_bindings,
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT & properties,
    std::span<std::byte> mapped_memory) noexcept
{
    for (uint32_t binding_index = 0u; binding_index < layout_bindings.size(); ++binding_index) {
        const auto & layout_binding = layout_bindings[binding_index];
        const auto & binding_authority = authority_bindings[layout_binding.binding];
        const vk::DeviceSize binding_offset = layout_offsets[binding_index];
        if (layout_binding.descriptorType == vk::DescriptorType::eCombinedImageSampler and
            layout_binding.descriptorCount > 1u and not properties.combinedImageSamplerDescriptorSingleArray) {
            write_combined_image_sampler_array(device, binding_authority, mapped_memory, binding_offset, properties);
            continue;
        }
        const std::size_t descriptor_size = get_descriptor_size(layout_binding.descriptorType, properties);
        for (uint32_t array_index = 0u; array_index < layout_binding.descriptorCount; ++array_index) {
            auto descriptor_get_info = make_descriptor_get_info(
                layout_binding.descriptorType,
                binding_authority[array_index].m_descriptor_info);
            device.getDescriptorEXT(
                descriptor_get_info,
                descriptor_size,
                mapped_memory.data() + binding_offset + array_index * descriptor_size);
        }
    }
}

} // anonymous namespace

std::error_code DescriptorSetProxy::bind(CommandBufferProxy & cmd, vk::PipelineBindPoint bind_point, vk::PipelineLayout pipeline_layout) noexcept
{
    if (not m_allocator_p) { return std::make_error_code(std::errc::invalid_argument); }
    const auto & properties = m_allocator_p->m_descriptor_buffer_properties;
    if (m_descriptor_set_buffer_version != m_authority_version) {
        vk::BufferCreateInfo staging_buffer_info;
        staging_buffer_info.setSize(m_layout.getLayoutSize())
            .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
            .setSharingMode(vk::SharingMode::eExclusive);
        MemoryAllocationInfo staging_allocation_info;
        staging_allocation_info.setAccess(MemoryAccess::eHostSequentialWrite);
        vkc::Buffer staging_buffer;
        const auto & memory_allocator = *m_allocator_p->m_memory_allocator_p;
        if (auto ec = staging_buffer.create(memory_allocator, staging_buffer_info, staging_allocation_info)) { return ec; }
        write_descriptors(
            memory_allocator.getDevice(),
            m_layout.getBindings(),
            m_layout.getLayoutOffsets(),
            m_authority_bindings,
            properties,
            staging_buffer.getMappedMemorySpan());
        if (auto ec = staging_buffer.flush()) { return ec; }

        vk::BufferMemoryBarrier2 to_transfer_write;
        to_transfer_write.setSrcStageMask(vk::PipelineStageFlagBits2::eAllCommands)
            .setSrcAccessMask(vk::AccessFlagBits2::eDescriptorBufferReadEXT)
            .setDstStageMask(vk::PipelineStageFlagBits2::eCopy)
            .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setBuffer(m_descriptor_set_buffer.handle())
            .setOffset(0u)
            .setSize(m_layout.getLayoutSize());
        vk::DependencyInfo to_transfer_write_dependency;
        to_transfer_write_dependency.setBufferMemoryBarriers(to_transfer_write);
        cmd.pipelineBarrier2(to_transfer_write_dependency);

        vk::BufferCopy copy_region {0u, 0u, m_layout.getLayoutSize()};
        cmd.copyBuffer(staging_buffer.handle(), m_descriptor_set_buffer.handle(), copy_region);

        vk::BufferMemoryBarrier2 to_descriptor_read;
        to_descriptor_read.setSrcStageMask(vk::PipelineStageFlagBits2::eCopy)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eAllCommands)
            .setDstAccessMask(vk::AccessFlagBits2::eDescriptorBufferReadEXT)
            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setBuffer(m_descriptor_set_buffer.handle())
            .setOffset(0u)
            .setSize(m_layout.getLayoutSize());
        vk::DependencyInfo to_descriptor_read_dependency;
        to_descriptor_read_dependency.setBufferMemoryBarriers(to_descriptor_read);
        cmd.pipelineBarrier2(to_descriptor_read_dependency);
        cmd.pinLease(staging_buffer.lease());
        m_descriptor_set_buffer_version = m_authority_version;
    }

    vk::DescriptorBufferBindingInfoEXT binding_info;
    binding_info.setAddress(m_descriptor_set_buffer.getDeviceAddress())
        .setUsage(vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
            vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT);
    cmd.bindDescriptorBuffersEXT(binding_info);
    const std::array buffer_indices {0u};
    const std::array offsets {vk::DeviceSize {0u}};
    cmd.setDescriptorBufferOffsetsEXT(bind_point, pipeline_layout, m_set_index, buffer_indices, offsets);
    cmd.pinLease(m_descriptor_set_buffer.lease());
    for (const auto & bindings : m_authority_bindings) {
        for (const auto & binding : bindings) { cmd.pinLeases(binding.m_resource_leases); }
    }
    return {};
}

} // namespace lcf::vkc::dsb
