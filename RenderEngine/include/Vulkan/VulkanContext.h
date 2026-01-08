#pragma once

#include "common/Context.h"
#include "Entity.h"
#include "VulkanSwapchain.h"
#include "VulkanMemoryAllocator.h"
#include "VulkanSamplerManager.h"
#include "VulkanDescriptorSetAllocator.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <unordered_map>

namespace lcf::render {
    class VulkanContext : public Context
    {
        using Self = VulkanContext;
    public:
        using SurfaceRenderTargetList = std::vector<VulkanSwapchain::SharedPointer>;
        using QueueFamilyIndexMap = std::unordered_map<vk::QueueFlagBits, uint32_t>;
        using QueueList = std::vector<vk::Queue>;
        using QueueListMap = std::unordered_map<vk::QueueFlagBits, QueueList>;
        using CommandBufferPoolMap = std::unordered_map<vk::QueueFlagBits, vk::UniqueCommandPool>;
        VulkanContext();
        VulkanContext(const VulkanContext &other) = delete;
        VulkanContext & operator=(const VulkanContext &other) = delete;
        ~VulkanContext();
        Self & registerWindow(Entity & window_entity);
        void create();
        bool isCreated() const { return m_device.get(); }
        bool isValid() const { return m_device.get(); }
        const vk::Instance & getInstance() const noexcept { return m_instance.get(); }
        vk::PhysicalDevice getPhysicalDevice() const noexcept { return m_physical_device; }
        const vk::Device & getDevice() const noexcept { return m_device.get(); }
        uint32_t getQueueFamilyIndex(vk::QueueFlagBits type) const noexcept;
        const vk::Queue & getQueue(vk::QueueFlagBits type) const noexcept;
        std::span<const vk::Queue> getSubQueues(vk::QueueFlagBits type) const noexcept;
        const vk::CommandPool & getCommandPool(vk::QueueFlagBits queue_type) const noexcept;
        const VulkanMemoryAllocator & getMemoryAllocator() const noexcept { return m_memory_allocator; }
        const VulkanDescriptorSetAllocator & getDescriptorSetAllocator() const noexcept { return m_descriptor_set_allocator; }
        const VulkanSamplerManager & getSamplerManager() const noexcept { return m_sampler_manager; }
    private:
        void setupVulkanInstance();
        void pickPhysicalDevice();
        int calculatePhysicalDeviceScore(const vk::PhysicalDevice &device);
        void findQueueFamilies();
        void createLogicalDevice();
        void createCommandPools();
    private:
        vk::UniqueInstance m_instance;
        vk::PhysicalDevice m_physical_device;
        vk::UniqueDevice m_device;
    #ifndef NDEBUG
        vk::UniqueDebugUtilsMessengerEXT m_debug_messenger;
    #endif
        SurfaceRenderTargetList m_surface_render_targets;
        QueueFamilyIndexMap m_queue_family_indices;
        QueueListMap m_queue_lists;
        CommandBufferPoolMap m_command_pools;
        VulkanMemoryAllocator m_memory_allocator;
        VulkanDescriptorSetAllocator m_descriptor_set_allocator;
        VulkanSamplerManager m_sampler_manager;
    };
}