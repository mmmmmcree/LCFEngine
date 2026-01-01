#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanDescriptorSet.h"
#include "VulkanImage.h"
#include "VulkanSampler.h"
#include "VulkanBufferObject.h"
#include <bitset>
#include <array>

namespace lcf::render {
    class VulkanContext;

    class VulkanMaterial
    {
        using Self = VulkanMaterial;
    public:
        using BindingViewList = std::span<const vk::DescriptorSetLayoutBinding>;
        using BindingList = std::vector<vk::DescriptorSetLayoutBinding>;
        using TextureList = std::vector<VulkanImage::SharedPointer>;
        using SamplerList = std::vector<VulkanSampler::SharedPointer>;
        inline static constexpr uint32_t s_max_binding_count = 8;
        using BindingTextureMap = std::array<TextureList, s_max_binding_count>;
        using BindingSamplerMap = std::array<SamplerList, s_max_binding_count>;       
        using BindingParamsSSBO = std::pair<uint32_t, VulkanBufferObject::SharedPointer>;
        using DirtyBindingMap = std::bitset<s_max_binding_count>;
        VulkanMaterial() = default;
        ~VulkanMaterial() = default;
        bool create(const VulkanDescriptorSet::SharedPointer & descriptor_set_sp); //todo create with MaterialCreateInfo, which has param like PBR or Phong ...
        const VulkanDescriptorSet & getDescriptorSet() const noexcept { return *m_descriptor_set_sp; }
        Self & setTexture(uint32_t binding, uint32_t index, const VulkanImage::SharedPointer & texture);
        Self & setSampler(uint32_t binding, uint32_t index, const VulkanSampler::SharedPointer & sampler);
        Self & setParamsSSBO(uint32_t binding, const VulkanBufferObject::SharedPointer & params_ssbo);
        void commitUpdate();
    private:
        VulkanDescriptorSet::SharedPointer m_descriptor_set_sp;
        DirtyBindingMap m_dirty_bindings;
        BindingTextureMap m_texture_map;
        BindingSamplerMap m_sampler_map;
        BindingParamsSSBO m_binding_params_ssbo {-1u, nullptr};
    };
}