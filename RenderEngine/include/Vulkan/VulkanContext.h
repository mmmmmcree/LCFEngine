#pragma once

#include "RHI/Context.h"
#include "VulkanSwapchain.h"
#include "VulkanMemoryAllocator.h"
#include "RenderWindow.h"
#include <vulkan/vulkan.hpp>
#include <QVulkanInstance>
#include <vector>
#include <unordered_map>
#include <stack>


namespace lcf::render {
    class VulkanContext : public Context
    {
    public:
        using SurfaceRenderTargetList = std::vector<VulkanSwapchain::SharedPointer>;
        using QueueFamilyIndexMap = std::unordered_map<uint32_t, uint32_t>; // <vk::QueueFlagBits, index>
        using QueueMap = std::unordered_map<uint32_t, vk::Queue>; // <vk::QueueFlagBits, vk::Queue>
        using CommandBufferStack = std::stack<vk::CommandBuffer>;
        VulkanContext();
        ~VulkanContext();
        void registerWindow(RenderWindow * window);
        void create();
        bool isCreated() const { return m_device.get(); }
        bool isValid() const { return m_device.get(); }
        // const vk::DispatchLoaderDynamic & getDispatchLoader() const { return *m_dispatch_loader; }
        vk::Instance getInstance() const { return m_vk_instance.vkInstance(); }
        vk::PhysicalDevice getPhysicalDevice() const { return m_physical_device; }
        vk::Device getDevice() const { return m_device.get(); }
        vk::CommandPool getCommandPool() const { return m_command_pool.get(); }
        uint32_t getQueueFamilyIndex(vk::QueueFlags flags) const { return m_queue_family_indices.at(static_cast<uint32_t>(flags)); }
        vk::Queue getQueue(vk::QueueFlags flags) const { return m_queues.at(static_cast<uint32_t>(flags)); }
        VulkanMemoryAllocator * getMemoryAllocator() { return &m_memory_allocator; }
        void bindCommandBuffer(vk::CommandBuffer command_buffer) { m_bound_cmd_buffer_stack.emplace(command_buffer); }
        vk::CommandBuffer getCurrentCommandBuffer() const;
        void releaseCommandBuffer() { m_bound_cmd_buffer_stack.pop(); }
    private:
        void setupVulkanInstance();
        void pickPhysicalDevice();
        int calculatePhysicalDeviceScore(const vk::PhysicalDevice &device);
        void findQueueFamilies();
        void createLogicalDevice();
        void createCommandPool();
    private:
        QVulkanInstance m_vk_instance;
        vk::PhysicalDevice m_physical_device;
        uint32_t m_host_visible_memory_index = -1u;
        uint32_t m_device_local_memory_index = -1u;
        vk::UniqueDevice m_device;
        SurfaceRenderTargetList m_surface_render_targets;
        QueueFamilyIndexMap m_queue_family_indices;
        QueueMap m_queues;
        vk::UniqueCommandPool m_command_pool;
        VulkanMemoryAllocator m_memory_allocator;
        CommandBufferStack m_bound_cmd_buffer_stack;
    };
}