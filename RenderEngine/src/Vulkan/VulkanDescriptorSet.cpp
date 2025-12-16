#include "vulkan/VulkanDescriptorSet.h"
#include "vulkan/VulkanContext.h"

using namespace lcf::render;

lcf::render::VulkanDescriptorSet::~VulkanDescriptorSet()
{
    if (not m_layout_sp) { return; }
    auto context_p = m_layout_sp->m_context_p;
    if (not m_descriptor_pool_opt.has_value()) { return; }
    auto device = context_p->getDevice();
    device.freeDescriptorSets(*m_descriptor_pool_opt, this->getHandle());
}

bool lcf::render::VulkanDescriptorSet::create(const VulkanDescriptorSetLayout::SharedPointer &layout_sp)
{
    m_layout_sp = layout_sp;
    auto context_p = m_layout_sp->m_context_p;
    auto & allocator = context_p->getDescriptorSetAllocator();
    allocator.allocate(*this);
    this->setIndex(layout_sp->getIndex());
    return m_descriptor_set;
}

VulkanDescriptorSetUpdater lcf::render::VulkanDescriptorSet::generateUpdater() const noexcept
{
    return VulkanDescriptorSetUpdater {
        this->getLayout().m_context_p,
        this->getHandle(),
        this->getLayout().getBindings()};
}