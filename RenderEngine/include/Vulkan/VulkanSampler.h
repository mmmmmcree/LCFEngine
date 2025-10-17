#pragma once

#include <vulkan/vulkan.hpp>
#include "PointerDefs.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanSampler : public STDPointerDefs<VulkanSampler>
    {
    public:
        VulkanSampler() = default;
        bool create(VulkanContext * context_p, const vk::SamplerCreateInfo & create_info);
        bool isCreated() const noexcept { return static_cast<bool>(m_sampler); }
        vk::Sampler getHandle() const noexcept { return m_sampler.get(); }
        vk::Filter getMinificationFilter() const noexcept { return m_min_filter; }
        vk::Filter getMagnificationFilter() const noexcept { return m_mag_filter; }
        vk::SamplerAddressMode getAddressModeU() const noexcept { return m_address_mode_u; }
        vk::SamplerAddressMode getAddressModeV() const noexcept { return m_address_mode_v; }
        vk::SamplerAddressMode getAddressModeW() const noexcept { return m_address_mode_w; }
    private:
        vk::UniqueSampler m_sampler;
        vk::Filter m_min_filter;
        vk::Filter m_mag_filter;
        vk::SamplerAddressMode m_address_mode_u;
        vk::SamplerAddressMode m_address_mode_v;
        vk::SamplerAddressMode m_address_mode_w;
    }; //todo generate hash value, managed by SamplerManager?
}