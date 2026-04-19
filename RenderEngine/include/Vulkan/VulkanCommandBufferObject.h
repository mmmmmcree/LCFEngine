#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_fwd_decls.h"
#include "VulkanTimelineSemaphore.h"
#include "resource_utils.h"
#include <boost/container/vector.hpp>

namespace lcf::render {
    class VulkanCommandBufferObject : public vk::CommandBuffer
    {
        using Base = vk::CommandBuffer;
        using Self = VulkanCommandBufferObject;
    public:
        using SemaphoreSubmitInfoList = boost::container::vector<vk::SemaphoreSubmitInfo>;
        using ResourceLeases = boost::container::vector<ResourceLease>;
        VulkanCommandBufferObject() = default;
        VulkanCommandBufferObject(const Self &) = default;
        Self & operator=(const Self &) = default;
        ~VulkanCommandBufferObject() noexcept;
        std::error_code create(VulkanContext * context_p, vk::QueueFlagBits queue_type);
        vk::QueueFlagBits getQueueType() const noexcept { return m_queue_type; }
        void waitUntilAvailable();
        void begin(const vk::CommandBufferBeginInfo & begin_info);
        void end();
        Self & addWaitSubmitInfo(const vk::SemaphoreSubmitInfo & wait_info) { m_wait_infos.emplace_back(wait_info); return *this; }
        Self & addSignalSubmitInfo(const vk::SemaphoreSubmitInfo & signal_info) { m_signal_infos.emplace_back(signal_info); return *this; }
        vk::SemaphoreSubmitInfo submit();
        void acquireResourceLease(ResourceLease resource_lease);
        void bindPipeline(const VulkanPipeline & pipeline) const noexcept;
        void bindDescriptorSet(const VulkanPipeline & pipeline, const VulkanDescriptorSet & descriptor_set) const noexcept;
        void bindDescriptorSet(const VulkanPipeline & pipeline, const VulkanBindlessDescriptorSet & descriptor_set) const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        vk::QueueFlagBits m_queue_type;
        std::shared_ptr<VulkanTimelineSemaphore> m_timeline_semaphore_sp;
        SemaphoreSubmitInfoList m_wait_infos;
        SemaphoreSubmitInfoList m_signal_infos;
        ResourceLeases m_resource_leases;
    };
}