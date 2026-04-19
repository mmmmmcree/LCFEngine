#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_enums.h"
#include "Vulkan/ds/VulkanDescriptorSetBinding.h"
#include <vector>
#include <variant>
#include <span>
#include <cstdint>

namespace lcf::render {

    class VulkanDescriptorSet
    {
        using Self = VulkanDescriptorSet;
        using BindingList = std::vector<VulkanDescriptorSetBinding>;
        using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
        struct PendingWrite
        {
            uint32_t binding;
            uint32_t array_index;
            DescriptorInfo info;
        };
    public:
        VulkanDescriptorSet() = default;
        VulkanDescriptorSet(
            vk::DescriptorSet handle,
            std::span<const VulkanDescriptorSetBinding> bindings,
            vkenums::DescriptorSetStrategy strategy,
            uint32_t set_index);
        ~VulkanDescriptorSet() = default;
        VulkanDescriptorSet(Self && other) noexcept;
        Self & operator=(Self && other) noexcept;
        VulkanDescriptorSet(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        operator bool() const noexcept { return static_cast<bool>(m_descriptor_set); }
        operator vk::DescriptorSet() const noexcept { return this->getHandle(); }
    public:
        Self & addDescriptorInfo(uint32_t binding, const DescriptorInfo & info);
        Self & addDescriptorInfo(uint32_t binding, uint32_t array_index, const DescriptorInfo & info);
        void commitUpdate(vk::Device device);
        const vk::DescriptorSet & getHandle() const noexcept { return m_descriptor_set; }
        uint32_t getIndex() const noexcept { return m_set_index; }
        vkenums::DescriptorSetStrategy getStrategy() const noexcept { return m_strategy; }
        std::span<const VulkanDescriptorSetBinding> getBindings() const noexcept { return m_binding_list; }
    private:
        vk::DescriptorSet m_descriptor_set = nullptr;
        BindingList m_binding_list;
        vkenums::DescriptorSetStrategy m_strategy = vkenums::DescriptorSetStrategy::eIndividual;
        uint32_t m_set_index = 0u;
        std::vector<PendingWrite> m_pending_writes;
    };

}
