#include "vk_core/descriptor_set/pool/DescriptorSetProxy.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/descriptor_set/pool/DescriptorSetAllocator.h"
#include "vk_core/error.h"
#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/Image.h"
#include "vk_core/sampler/Sampler.h"
#include "vk_core/utils/enum/vk_enums_traits.h"
#include <cassert>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::dsp {

DescriptorSetProxy::DescriptorSet::operator DescriptorSetAllocation() const noexcept
{
    return {m_pool, static_cast<VkDescriptorPoolCreateFlags>(m_pool_key), { m_descriptor_set.get() } };
}

bool DescriptorSetProxy::DescriptorSet::isAvailable() const noexcept
{
    return m_descriptor_set.lease().getRefCount() == 2u;
}

DescriptorSetProxy::~DescriptorSetProxy() noexcept
{
    if (not m_allocator_p) { return; }
    for (auto & descriptor_set : m_available_descriptor_sets) {
        m_allocator_p->recycle(descriptor_set);
    }
    for (auto & descriptor_set : m_in_use_descriptor_sets) {
        m_allocator_p->recycle(descriptor_set, descriptor_set.lease());
    }
}

std::error_code DescriptorSetProxy::create(DescriptorSetAllocator & allocator, const DescriptorSetLayout & layout) noexcept
{
    if (m_allocator_p) { return make_error_code(errc::already_created); }
    if (not layout.handle()) { return std::make_error_code(std::errc::invalid_argument); }
    m_allocator_p = &allocator;
    m_layout = layout;
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
    auto & authority_binding = m_authority_bindings[binding][array_index];
    authority_binding.m_descriptor_info = vk::DescriptorBufferInfo {buffer.handle(), offset, range};
    authority_binding.m_resource_leases[0] = buffer.lease();
    ++m_version;
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
    ++m_version;
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
    ++m_version;
    return *this;
}

auto DescriptorSetProxy::setSampler(uint32_t binding, const vkc::Sampler & sampler) noexcept -> Self &
{
    return this->setSampler(binding, 0u, sampler);
}

namespace {

std::vector<vk::WriteDescriptorSet> make_writes(vk::DescriptorSet descriptor_set, const auto & layout_bindings, const auto & authority_bindings) noexcept
{
    std::vector<vk::WriteDescriptorSet> writes;
    writes.reserve(stdr::fold_left(
        layout_bindings | stdv::transform(&vk::DescriptorSetLayoutBinding::descriptorCount),
        uint32_t {0u}, std::plus<uint32_t> {}));
    for (const auto & layout_binding : layout_bindings) {
        const auto & binding_authority = authority_bindings[layout_binding.binding];
        for (uint32_t array_index = 0u; array_index < layout_binding.descriptorCount; ++array_index) {
            const auto & authority_binding = binding_authority[array_index];
            vk::WriteDescriptorSet write;
            write.setDstSet(descriptor_set)
                .setDstBinding(layout_binding.binding)
                .setDstArrayElement(array_index)
                .setDescriptorType(layout_binding.descriptorType);
            std::visit([&write](const auto & descriptor_info) {
                using DescriptorInfoType = std::decay_t<decltype(descriptor_info)>;
                if constexpr (std::is_same_v<DescriptorInfoType, vk::DescriptorBufferInfo>) {
                    write.setBufferInfo(descriptor_info);
                } else {
                    write.setImageInfo(descriptor_info);
                }
            }, authority_binding.m_descriptor_info);
            writes.emplace_back(write);
        }
    }
    return writes;
}

} // anonymous namespace

std::error_code DescriptorSetProxy::bind(CommandBufferProxy & cmd, vk::PipelineBindPoint bind_point, vk::PipelineLayout pipeline_layout) noexcept
{
    while (not m_in_use_descriptor_sets.empty()) {
        auto & descriptor_set = m_in_use_descriptor_sets.front();
        if (not descriptor_set.isAvailable()) { break; }
        m_available_descriptor_sets.emplace_back(std::move(descriptor_set));
        m_in_use_descriptor_sets.pop_front();
    }
    if (m_available_descriptor_sets.empty()) {
        auto allocation_result = m_allocator_p->allocate(m_layout, std::max(1u, static_cast<uint32_t>(m_in_use_descriptor_sets.size()) * 2));
        if (not allocation_result) { return allocation_result.error(); }
        auto allocation = std::move(*allocation_result);
        for (const auto & ds : allocation.m_descriptor_sets) {
            m_available_descriptor_sets.emplace_back(DescriptorSet {
                .m_pool = allocation.m_pool,
                .m_pool_key = vk::DescriptorPoolCreateFlags(allocation.m_pool_key),
                .m_descriptor_set = utils::ResourceHandle<vk::DescriptorSet>(ds)
            });
        }
    }
    DescriptorSet descriptor_set = std::move(m_available_descriptor_sets.back());
    m_available_descriptor_sets.pop_back();
    if (descriptor_set.m_version != m_version) {
        auto writes = make_writes(descriptor_set.handle(), m_layout.getBindings(), m_authority_bindings);
        m_allocator_p->m_device.updateDescriptorSets(writes, nullptr);
        descriptor_set.m_version = m_version;
    }
    cmd.bindDescriptorSets(bind_point, pipeline_layout, m_set_index, descriptor_set.handle(), nullptr);
    cmd.pinLease(descriptor_set.m_descriptor_set.lease());
    for (const auto & bindings : m_authority_bindings) {
        for (const auto & binding : bindings) { cmd.pinLeases(binding.m_resource_leases); }
    }
    m_in_use_descriptor_sets.emplace_back(std::move(descriptor_set));
    return {};
}

} // namespace lcf::vkc::dsp
