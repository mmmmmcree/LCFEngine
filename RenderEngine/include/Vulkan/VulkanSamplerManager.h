#pragma once

#include "Vulkan/VulkanSampler.h"
#include <vector>
#include <unordered_map>

namespace lcf::render {
    class VulkanContext;

    class VulkanSamplerManager
    {
        using Self = VulkanSamplerManager;
    public:
        using SamplerSharedPtrMap = std::unordered_map<uint64_t, std::shared_ptr<VulkanSampler>>;
        using Hasher = typename VulkanSamplerParams::Hasher;
        VulkanSamplerManager() = default;
        ~VulkanSamplerManager() = default;
        VulkanSamplerManager(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanSamplerManager(Self &&) = default;
        Self & operator=(Self &&) = default;
        void create(VulkanContext * context_p);
        const VulkanSampler & get(const VulkanSamplerParams & params) const;
        const VulkanSampler & get(SamplerPreset preset) const;
        const std::shared_ptr<VulkanSampler> & getShared(const VulkanSamplerParams & params) const;
        const std::shared_ptr<VulkanSampler> & getShared(SamplerPreset preset) const;
        bool contains(const VulkanSamplerParams & params) const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        mutable SamplerSharedPtrMap m_sampler_sp_map;
    };
}
