#pragma once

#include "RHI/Context.h"
#include "VulkanSwapchain.h"
#include "VulkanMemoryAllocator.h"
#include "RenderWindow.h"
#include <vulkan/vulkan.hpp>
#include <QVulkanInstance>
#include <vector>
#include <unordered_map>


namespace lcf::render {
    class VulkanContext : public Context
    {
    public:
        using SurfaceRenderTargetList = std::vector<VulkanSwapchain *>;
        using QueueFamilyIndexMap = std::unordered_map<uint32_t, uint32_t>; // <vk::QueueFlagBits, index>
        using QueueMap = std::unordered_map<uint32_t, vk::Queue>; // <vk::QueueFlagBits, vk::Queue>
        VulkanContext();
        ~VulkanContext() override;
        virtual void registerWindow(RenderWindow * window) override;
        virtual void unregisterWindow(RenderWindow * window) override;
        virtual void create() override;
        virtual bool isCreated() const override { return m_device.get(); }
        virtual bool isValid() const override { return m_device.get(); }
        vk::Instance getInstance() const { return m_vk_instance.vkInstance(); }
        vk::PhysicalDevice getPhysicalDevice() const { return m_physical_device; }
        vk::Device getDevice() const { return m_device.get(); }
        vk::CommandPool getCommandPool() const { return m_command_pool.get(); }
        uint32_t getQueueFamilyIndex(vk::QueueFlags flags) const { return m_queue_family_indices.at(static_cast<uint32_t>(flags)); }
        vk::Queue getQueue(vk::QueueFlags flags) const { return m_queues.at(static_cast<uint32_t>(flags)); }
        VulkanMemoryAllocator * getMemoryAllocator() { return &m_memory_allocator; }
    private:
        void setupVulkanInstance();
        void pickPhysicalDevice();
        int calculatePhysicalDeviceScore(const vk::PhysicalDevice &device);
        void findQueueFamilies();
        void createLogicalDevice();
        void createCommandPool();
    private:
        QVulkanInstance m_vk_instance;
        SurfaceRenderTargetList m_surface_render_targets;
        vk::PhysicalDevice m_physical_device;
        uint32_t m_host_visible_memory_index = -1u;
        uint32_t m_device_local_memory_index = -1u;
        vk::UniqueDevice m_device;
        QueueFamilyIndexMap m_queue_family_indices;
        QueueMap m_queues;
        vk::UniqueCommandPool m_command_pool;
        VulkanMemoryAllocator m_memory_allocator;
    };
}