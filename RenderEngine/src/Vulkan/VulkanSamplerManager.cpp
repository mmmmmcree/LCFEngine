#include "Vulkan/VulkanSamplerManager.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

void lcf::render::VulkanSamplerManager::create(VulkanContext *context_p)
{
    m_context_p = context_p;
}

const VulkanSampler & lcf::render::VulkanSamplerManager::get(const VulkanSamplerParams &params) const
{
    auto sampler_info = params.toCreateInfo();
    if (sampler_info.anisotropyEnable) {
        auto limits = m_context_p->getPhysicalDevice().getProperties().limits;
        sampler_info.maxAnisotropy = std::min(sampler_info.maxAnisotropy, limits.maxSamplerAnisotropy);
    }
    uint64_t hash_value = Hasher{}(params);
    auto it = m_sampler_map.find(hash_value);
    if (it != m_sampler_map.end()) { return it->second; }
    VulkanSampler sampler;
    sampler.create(m_context_p->getDevice(), params.toCreateInfo());
    return m_sampler_map.emplace(hash_value, std::move(sampler)).first->second;
}

const VulkanSampler &lcf::render::VulkanSamplerManager::get(SamplerPreset preset) const
{
    return this->get(VulkanSamplerParams::from_preset(preset));
}

bool lcf::render::VulkanSamplerManager::contains(const VulkanSamplerParams &params) const noexcept
{
    return m_sampler_map.contains(Hasher{}(params));
}
