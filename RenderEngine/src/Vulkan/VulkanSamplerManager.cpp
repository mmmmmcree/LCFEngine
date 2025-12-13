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

const VulkanSampler::SharedPointer &lcf::render::VulkanSamplerManager::getShared(const VulkanSamplerParams &params) const
{
    uint64_t hash_value = Hasher{}(params);
    auto it = m_sampler_sp_map.find(hash_value);
    if (it != m_sampler_sp_map.end()) { return it->second; }
    auto sampler_sp = VulkanSampler::makeShared();
    vk::SamplerCreateInfo sampler_info {
        {},
        params.getMinFilter(),
        params.getMagFilter() ,
        params.getMipmapMode(),
        params.getAddressModeU(),
        params.getAddressModeV(),
        params.getAddressModeW(),
        params.getMipLodBias(),
        params.getAnisotropyEnable(),
        params.getMaxAnisotropy(),
        params.getCompareEnable(),
        params.getCompareOp(),
        params.getMinLod(),
        params.getMaxLod(),
        params.getBorderColor(),
        params.isUnnormalizedCoordinates()
    };
    sampler_sp->create(m_context_p, sampler_info);
    return m_sampler_sp_map[hash_value] = sampler_sp;
}

bool lcf::render::VulkanSamplerManager::contains(const VulkanSamplerParams &params) const noexcept
{
    return m_sampler_sp_map.contains(Hasher{}(params));
}
