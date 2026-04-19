#pragma once

#include "Vulkan/VulkanSampler.h"
#include <vector>
#include <unordered_map>

namespace lcf::render {
    class VulkanContext;

    class VulkanSamplerGroup
    {
        using Self = VulkanSamplerGroup;
    public:
        using SamplerSharedPtrList = std::vector<std::shared_ptr<VulkanSampler>>;
        VulkanSamplerGroup() = default;
        ~VulkanSamplerGroup() = default;
        VulkanSamplerGroup(const Self &) = default;
        Self & operator=(const Self &) = default;
        VulkanSamplerGroup(Self &&) = default;
        Self & operator=(Self &&) = default;
        Self & add(const std::shared_ptr<VulkanSampler> & sampler_sp) { m_sampler_sp_list.emplace_back(sampler_sp); return *this; }
        const VulkanSampler & get(size_t index) const { return *m_sampler_sp_list.at(index); }
    private:
        SamplerSharedPtrList m_sampler_sp_list;
    };

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
