#include "Vulkan/ds/details/VulkanBindlessDescriptorSetAllocator.h"
#include "Vulkan/ds/VulkanDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout.h"

using namespace lcf::render;
using namespace lcf::render::detail;

VulkanBindlessDescriptorSetAllocator::~VulkanBindlessDescriptorSetAllocator() noexcept
{
    for (auto & [_, pool] : m_set_to_pool_map) {
        m_device.destroyDescriptorPool(pool);
    }
    m_set_to_pool_map.clear();
}

std::error_code VulkanBindlessDescriptorSetAllocator::create(vk::Device device) noexcept
{
    if (not device) { return std::make_error_code(std::errc::invalid_argument);  }
    m_device = device;
    return {};
}

VulkanBindlessDescriptorSetAllocator::AllocResult VulkanBindlessDescriptorSetAllocator::allocate(
    const VulkanDescriptorSetLayout &layout,
    uint32_t variable_count) noexcept
{
    auto strategy = layout.getStrategy();
    if (strategy != vkenums::DescriptorSetStrategy::eBindless) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    auto pool = this->createPool(layout, variable_count);
    vk::DescriptorSetAllocateInfo alloc_info;
    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_descriptor_count_info;
    variable_descriptor_count_info.setDescriptorCounts(variable_count);
    alloc_info.setDescriptorPool(pool)
        .setSetLayouts(layout.getHandle());
    auto bindings = layout.getBindings();
    if (bindings.containsFlags(vk::DescriptorBindingFlagBits::eVariableDescriptorCount)) {
        alloc_info.setPNext(&variable_descriptor_count_info);
    }
    vk::DescriptorSet descriptor_set;
    try {
        descriptor_set = m_device.allocateDescriptorSets(alloc_info).front();
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    m_set_to_pool_map[descriptor_set] = alloc_info.descriptorPool;
    return VulkanDescriptorSet { descriptor_set, layout.getBindings(), layout.getStrategy(), layout.getIndex() };
}

void VulkanBindlessDescriptorSetAllocator::deallocate(VulkanDescriptorSet &&set) noexcept
{
    if (set.getStrategy() != vkenums::DescriptorSetStrategy::eBindless) { return; }
    auto pool = m_set_to_pool_map[set.getHandle()];
    m_device.destroyDescriptorPool(pool);
    m_set_to_pool_map.erase(set.getHandle());
}

vk::DescriptorPool VulkanBindlessDescriptorSetAllocator::createPool(
    const VulkanDescriptorSetLayout &layout,
    uint32_t variable_count) noexcept
{
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    for (const auto & binding : layout.getBindings()) {
        pool_sizes.emplace_back(binding.getDescriptorType(), binding.getDescriptorCount());
    }
    if (layout.getBindings().containsFlags(vk::DescriptorBindingFlagBits::eVariableDescriptorCount)) {
        pool_sizes.back().descriptorCount = variable_count;
    }
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
        .setMaxSets(1)
        .setPoolSizes(pool_sizes);
    vk::DescriptorPool pool {};
    try {
        pool = m_device.createDescriptorPool(pool_info);
    } catch (const vk::SystemError &) {}
    return pool;
}
