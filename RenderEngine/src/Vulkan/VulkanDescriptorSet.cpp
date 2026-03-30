#include "Vulkan/VulkanDescriptorSet.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

lcf::render::VulkanDescriptorSet::~VulkanDescriptorSet()
{
    // if (not m_layout_sp) { return; }
    // auto context_p = m_layout_sp->m_context_p;
    if (not m_descriptor_pool_opt.has_value()) { return; }
    auto device = m_context_p->getDevice();
    device.freeDescriptorSets(*m_descriptor_pool_opt, this->getHandle());
}

bool lcf::render::VulkanDescriptorSet::create(const VulkanDescriptorSetLayout::SharedPointer &layout_sp)
{
    m_layout_sp = layout_sp;
    m_context_p = layout_sp->m_context_p;
    m_binding_list.assign_range(layout_sp->getBindings());
    // auto context_p = m_layout_sp->m_context_p;
    auto & allocator = m_context_p->getDescriptorSetAllocator();
    allocator.allocate(*this);
    this->setIndex(layout_sp->getIndex());
    return m_descriptor_set;
}

VulkanDescriptorSetUpdater lcf::render::VulkanDescriptorSet::generateUpdater() const noexcept
{
    return VulkanDescriptorSetUpdater {
        m_context_p->getDevice(),
        this->getHandle(),
        this->getBindings()};
}