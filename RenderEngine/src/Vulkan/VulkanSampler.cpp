#include "Vulkan/VulkanSampler.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

bool VulkanSampler::create(VulkanContext * context_p, const vk::SamplerCreateInfo &create_info)
{
    auto device = context_p->getDevice();
    vk::SamplerCreateInfo sampler_info = create_info;
    if (sampler_info.anisotropyEnable) {
        auto limits = context_p->getPhysicalDevice().getProperties().limits;
        sampler_info.maxAnisotropy = std::min(create_info.maxAnisotropy, limits.maxSamplerAnisotropy);
    }
    m_sampler = device.createSamplerUnique(sampler_info);
    m_min_filter = sampler_info.minFilter;
    m_mag_filter = sampler_info.magFilter;
    m_address_mode_u = sampler_info.addressModeU;
    m_address_mode_v = sampler_info.addressModeV;
    m_address_mode_w = sampler_info.addressModeW;
    return this->isCreated();
}