#pragma once

#include <vulkan/vulkan.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/vector.hpp>
#include <queue>
#include "VulkanTimelineSemaphore.h"
#include "common/GPUResource.h"

namespace lcf::render {
    class VulkanContext;
    class VulkanCommandBufferObject : public vk::CommandBuffer
    {
        using Self = VulkanCommandBufferObject;
    public:
        using SemaphoreSubmitInfoList = boost::container::vector<vk::SemaphoreSubmitInfo>;
        using GPUResourceList =  boost::container::vector<GPUResource::SharedPointer>;
        VulkanCommandBufferObject() = default;
        bool create(VulkanContext * context_p);
        void prepareForRecording();
        void begin(const vk::CommandBufferBeginInfo & begin_info);
        void end();
        Self & addWaitSubmitInfo(const vk::SemaphoreSubmitInfo & wait_info) { m_wait_infos.emplace_back(wait_info); return *this; }
        Self & addSignalSubmitInfo(const vk::SemaphoreSubmitInfo & signal_info) { m_signal_infos.emplace_back(signal_info); return *this; }
        void submit(vk::QueueFlags queue_flags);
        const VulkanTimelineSemaphore::SharedPointer & getTimelineSemaphore() const noexcept { return m_timeline_semaphore_sp; }
        void acquireResource(const GPUResource::SharedPointer & resource_sp);
    private:
        VulkanContext * m_context_p = nullptr;
        VulkanTimelineSemaphore::SharedPointer m_timeline_semaphore_sp;
        SemaphoreSubmitInfoList m_wait_infos;
        SemaphoreSubmitInfoList m_signal_infos;
        GPUResourceList m_resources;
    };
}