#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_enums.h"
#include "VulkanDescriptorSetBinding.h"
#include <vector>
#include <variant>
#include <span>
#include <cstdint>

namespace lcf::render {

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
            vk::DescriptorSet handle,
            std::span<const VulkanDescriptorSetBinding> bindings,
            vkenums::DescriptorSetStrategy strategy,
            uint32_t set_index);
        ~VulkanDescriptorSet2() = default;
        VulkanDescriptorSet2(Self && other) noexcept;
        Self & operator=(Self && other) noexcept;
        VulkanDescriptorSet2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        operator bool() const noexcept { return static_cast<bool>(m_descriptor_set); }

    public:
        Self & addDescriptorInfo(uint32_t binding, const vk::DescriptorBufferInfo & info);
        Self & addDescriptorInfo(uint32_t binding, uint32_t array_index, const vk::DescriptorBufferInfo & info);
        Self & addDescriptorInfo(uint32_t binding, const vk::DescriptorImageInfo & info);
        Self & addDescriptorInfo(uint32_t binding, uint32_t array_index, const vk::DescriptorImageInfo & info);
        void   commitUpdate(vk::Device device);

        const vk::DescriptorSet & getHandle() const noexcept { return m_descriptor_set; }
        uint32_t getIndex() const noexcept { return m_set_index; }
        vkenums::DescriptorSetStrategy getStrategy() const noexcept { return m_strategy; }
        std::span<const VulkanDescriptorSetBinding> getBindings() const noexcept { return m_binding_list; }

    private:
        vk::DescriptorSet m_descriptor_set = nullptr;
        BindingList m_binding_list;
        vkenums::DescriptorSetStrategy m_strategy = vkenums::DescriptorSetStrategy::ePerFrame;
        uint32_t m_set_index = 0u;
        std::vector<PendingWrite> m_pending_writes;
    };

}
