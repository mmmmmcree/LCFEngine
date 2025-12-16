#pragma once

#include "vulkan/VulkanSampler.h"
#include <vector>
#include <unordered_map>

namespace lcf::render {
    class VulkanContext;

    class VulkanSamplerGroup
    {
        using Self = VulkanSamplerGroup;
    public:
        using SamplerSharedPtrList = std::vector<typename VulkanSampler::SharedPointer>;
        VulkanSamplerGroup() = default;
        ~VulkanSamplerGroup() = default;
        VulkanSamplerGroup(const Self &) = default;
        Self & operator=(const Self &) = default;
        VulkanSamplerGroup(Self &&) = default;
        Self & operator=(Self &&) = default;
        Self & add(const VulkanSampler::SharedPointer & sampler_sp) { m_sampler_sp_list.emplace_back(sampler_sp); return *this; }
        const VulkanSampler & get(size_t index) const { return *m_sampler_sp_list.at(index); }
    private:
        SamplerSharedPtrList m_sampler_sp_list;
    };

    class VulkanSamplerManager
    {
        using Self = VulkanSamplerManager;
    public:
        using SamplerSharedPtrMap = std::unordered_map<uint64_t, typename VulkanSampler::SharedPointer>;
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
        const VulkanSampler::SharedPointer & getShared(const VulkanSamplerParams & params) const;
        const VulkanSampler::SharedPointer & getShared(SamplerPreset preset) const;
        bool contains(const VulkanSamplerParams & params) const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        mutable SamplerSharedPtrMap m_sampler_sp_map;
    };
}
