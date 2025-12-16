#include "vulkan/VulkanSamplerManager.h"

using namespace lcf::render;

void lcf::render::VulkanSamplerManager::create(VulkanContext *context_p)
{
    m_context_p = context_p;
}

const VulkanSampler & lcf::render::VulkanSamplerManager::get(const VulkanSamplerParams &params) const
{
    return *(this->getShared(params));
}

const VulkanSampler &lcf::render::VulkanSamplerManager::get(SamplerPreset preset) const
{
    return this->get(VulkanSamplerParams::from_preset(preset));
}

const VulkanSampler::SharedPointer &lcf::render::VulkanSamplerManager::getShared(const VulkanSamplerParams &params) const
{
    uint64_t hash_value = Hasher{}(params);
    auto it = m_sampler_sp_map.find(hash_value);
    if (it != m_sampler_sp_map.end()) { return it->second; }
    auto sampler_sp = VulkanSampler::makeShared();
    sampler_sp->create(m_context_p, params.toCreateInfo());
    return m_sampler_sp_map[hash_value] = sampler_sp;
}

const VulkanSampler::SharedPointer &lcf::render::VulkanSamplerManager::getShared(SamplerPreset preset) const
{
    return this->getShared(VulkanSamplerParams::from_preset(preset));
}

bool lcf::render::VulkanSamplerManager::contains(const VulkanSamplerParams &params) const noexcept
{
    return m_sampler_sp_map.contains(Hasher{}(params));
}
