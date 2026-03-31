#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_enums.h"
#include "VulkanDescriptorSetBinding.h"
#include <vector>
#include <variant>
#include <span>
#include <cstdint>

namespace lcf::render {

    class VulkanContext;

    class VulkanDescriptorSet2
    {
        using Self = VulkanDescriptorSet2;
        using BindingList = std::vector<VulkanDescriptorSetBinding>;
        using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
        struct PendingWrite
        {
            uint32_t binding;
            uint32_t array_index;
            DescriptorInfo info;
        };
    public:
        VulkanDescriptorSet2() = default;
        VulkanDescriptorSet2(
            VulkanContext * context_p,
            vk::DescriptorSet handle,
            std::span<const VulkanDescriptorSetBinding> bindings,
            vkenums::DescriptorSetStrategy strategy,
            uint32_t set_index);
        ~VulkanDescriptorSet2();
        VulkanDescriptorSet2(Self && other) noexcept;
        Self & operator=(Self && other) noexcept;
        VulkanDescriptorSet2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
    public:
        Self & addDescriptorInfo(uint32_t binding, const vk::DescriptorBufferInfo & info);
        Self & addDescriptorInfo(uint32_t binding, uint32_t array_index, const vk::DescriptorBufferInfo & info);
        Self & addDescriptorInfo(uint32_t binding, const vk::DescriptorImageInfo & info);
        Self & addDescriptorInfo(uint32_t binding, uint32_t array_index, const vk::DescriptorImageInfo & info);
        void   commitUpdate();
        const vk::DescriptorSet & getHandle() const noexcept { return m_descriptor_set; }
        uint32_t getIndex() const noexcept { return m_set_index; }
        vkenums::DescriptorSetStrategy getStrategy() const noexcept { return m_strategy; }
        std::span<const VulkanDescriptorSetBinding> getBindings() const noexcept { return m_binding_list; }
        bool isValid() const noexcept { return static_cast<bool>(m_descriptor_set); }
        VulkanContext * getContext() const noexcept { return m_context_p; }
    private:
        void release();
    private:
        VulkanContext * m_context_p = nullptr;
        vk::DescriptorSet m_descriptor_set;
        BindingList m_binding_list;
        vkenums::DescriptorSetStrategy m_strategy = vkenums::DescriptorSetStrategy::ePerFrame;
        uint32_t m_set_index = 0u;
        std::vector<PendingWrite> m_pending_writes;
    };

}
