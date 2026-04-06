#include "Vulkan/ds/VulkanDescriptorSetManager.h"
#include "Vulkan/ds/details/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_constants.h"
#include "enums/enum_count.h"

using namespace lcf::render;
namespace stdv = std::views;

std::error_code VulkanDescriptorSetManager::create(VulkanContext & context) noexcept
{
    m_context_p = &context;
    vk::Device device = m_context_p->getDevice();
    m_allocator_up = std::make_unique<detail::VulkanDescriptorSetAllocator2>();
    if (auto ec = m_allocator_up->create(device)) { return ec; }
}

VulkanDescriptorSet2 VulkanDescriptorSetManager::createSet(const VulkanDescriptorSetLayout2 & layout)
{
    auto result = m_allocator_up->allocate(layout);
    if (not result) { return {}; }
    return std::move(*result);
}

void VulkanDescriptorSetManager::destroySet(VulkanDescriptorSet2 && ds)
{
    if (not ds) { return; }
    m_allocator_up->deallocate(std::move(ds));
}

std::error_code lcf::render::VulkanDescriptorSetManager::initBindlessSets(uint32_t frame_copys)
{
    auto device = m_context_p->getDevice();
    if (auto ec = m_bindless_buffer_set.create(device, vkenums::BindlessSetType::eBuffer, frame_copys)) { return ec; }
    if (auto ec = m_bindless_texture_set.create(device, vkenums::BindlessSetType::eTexture, frame_copys)) { return ec; }
    return {};
}