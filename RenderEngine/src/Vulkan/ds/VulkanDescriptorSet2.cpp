#include "Vulkan/ds/VulkanDescriptorSet2.h"
#include <utility>
#include <ranges>

using namespace lcf::render;

VulkanDescriptorSet2::VulkanDescriptorSet2(
    vk::DescriptorSet handle,
    std::span<const VulkanDescriptorSetBinding> bindings,
    vkenums::DescriptorSetStrategy strategy,
    uint32_t set_index) :
    m_descriptor_set(handle),
    m_binding_list(bindings.begin(), bindings.end()),
    m_strategy(strategy),
    m_set_index(set_index)
{
}

VulkanDescriptorSet2::VulkanDescriptorSet2(Self && other) noexcept :
    m_descriptor_set(std::exchange(other.m_descriptor_set, nullptr)),
    m_binding_list(std::move(other.m_binding_list)),
    m_strategy(other.m_strategy),
    m_set_index(other.m_set_index),
    m_pending_writes(std::move(other.m_pending_writes))
{
}

VulkanDescriptorSet2 & VulkanDescriptorSet2::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    m_descriptor_set = std::exchange(other.m_descriptor_set, nullptr);
    m_binding_list = std::move(other.m_binding_list);
    m_strategy = other.m_strategy;
    m_set_index = other.m_set_index;
    m_pending_writes = std::move(other.m_pending_writes);
    return *this;
}

VulkanDescriptorSet2 & VulkanDescriptorSet2::addDescriptorInfo(uint32_t binding, const vk::DescriptorBufferInfo & info)
{
    return this->addDescriptorInfo(binding, 0u, info);
}

VulkanDescriptorSet2 & VulkanDescriptorSet2::addDescriptorInfo(uint32_t binding, uint32_t array_index, const vk::DescriptorBufferInfo & info)
{
    m_pending_writes.emplace_back(binding, array_index, DescriptorInfo{info});
    return *this;
}

VulkanDescriptorSet2 & VulkanDescriptorSet2::addDescriptorInfo(uint32_t binding, const vk::DescriptorImageInfo & info)
{
    return this->addDescriptorInfo(binding, 0u, info);
}

VulkanDescriptorSet2 & VulkanDescriptorSet2::addDescriptorInfo(uint32_t binding, uint32_t array_index, const vk::DescriptorImageInfo & info)
{
    m_pending_writes.emplace_back(binding, array_index, DescriptorInfo{info});
    return *this;
}

void VulkanDescriptorSet2::commitUpdate(vk::Device device)
{
    if (m_pending_writes.empty() || not device) { return; }

    auto to_write = [this](PendingWrite & pw) -> vk::WriteDescriptorSet {
        const auto & b = m_binding_list[pw.binding];
        vk::WriteDescriptorSet w;
        w.setDstSet(m_descriptor_set)
            .setDstBinding(b.binding)
            .setDstArrayElement(pw.array_index)
            .setDescriptorType(b.descriptorType);
        std::visit([&w](auto & info) {
            using T = std::decay_t<decltype(info)>;
            if constexpr (std::is_same_v<T, vk::DescriptorBufferInfo>) { w.setBufferInfo(info); }
            else { w.setImageInfo(info); }
        }, pw.info);
        return w;
    };

    auto writes = m_pending_writes | std::views::transform(to_write) | std::ranges::to<std::vector>();
    device.updateDescriptorSets(writes, nullptr);
    m_pending_writes.clear();
}
