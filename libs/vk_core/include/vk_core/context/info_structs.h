#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/bootstrap/info_structs.h"
#include "vk_core/context/enums.h"

namespace lcf::vkc {

using InstanceContextCreateInfo = bs::InstanceCreateInfo;

class QueueRequest
{
    using Self = QueueRequest;
public:
    QueueRequest(
        vk::QueueFlags required_flags = {},
        vk::QueueFlags undesired_flags = {},
        QueueSubmissionThreadTag submission_thread_tag = {},
        float priority = 1.0f,
        vk::SurfaceKHR present_surface = nullptr) noexcept :
        m_required_flags(required_flags),
        m_undesired_flags(undesired_flags),
        m_submission_thread_tag(submission_thread_tag),
        m_priority(priority),
        m_present_surface(present_surface) {}
    Self & addRequiredFlags(vk::QueueFlags flags) noexcept { m_required_flags |= flags; return *this; }
    Self & addUndesiredFlags(vk::QueueFlags flags) noexcept { m_undesired_flags |= flags; return *this; }
    Self & setPresentSurface(vk::SurfaceKHR surface) noexcept { m_present_surface = surface; return *this; }
    Self & setSubmissionThreadTag(QueueSubmissionThreadTag tag) noexcept { m_submission_thread_tag = tag; return *this; }
    Self & setPriority(float priority) noexcept { priority = priority; return *this; }
    const vk::QueueFlags & getRequiredFlags() const noexcept { return m_required_flags; }
    const vk::QueueFlags & getUndesiredFlags() const noexcept { return m_undesired_flags; }
    const vk::SurfaceKHR & getPresentSurface() const noexcept { return m_present_surface; }
    const QueueSubmissionThreadTag & getSubmissionThreadTag() const noexcept { return m_submission_thread_tag; }
    const float & getPriority() const noexcept { return m_priority; }
private:
    vk::QueueFlags m_required_flags;
    vk::QueueFlags m_undesired_flags;
    QueueSubmissionThreadTag m_submission_thread_tag;
    float m_priority;
    vk::SurfaceKHR m_present_surface;
};

class DeviceContextCreateInfo
{
    using Self = DeviceContextCreateInfo;
    using QueueRequestList = std::vector<QueueRequest>;
public:
    ~DeviceContextCreateInfo() noexcept = default;
    DeviceContextCreateInfo() noexcept = default;
    DeviceContextCreateInfo(const Self &) = delete;
    DeviceContextCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) noexcept = default;
public:
    Self & setPhysicalDeviceSelectInfo(const bs::PhysicalDeviceSelectInfo & info) noexcept
    {
        m_physical_device_select_info = info;
        return *this;
    }
    Self & setRequiredDeviceExtensionManifest(const DeviceExtensionManifest & manifest) noexcept
    {
        m_device_create_info.setRequiredDeviceExtensionManifest(manifest);   
        return *this;
    }
    QueueKey addQueueRequest(const QueueRequest & request) noexcept
    {
        m_queue_requests.push_back(request);
        return QueueKey {static_cast<std::underlying_type_t<QueueKey>>(m_queue_requests.size()) - 1};
    }
    const bs::PhysicalDeviceSelectInfo & getPhysicalDeviceSelectInfo() const noexcept { return m_physical_device_select_info; }
    const bs::DeviceCreateInfo & getDeviceCreateInfo() const noexcept { return m_device_create_info; }   
    const QueueRequestList & getQueueRequests() const noexcept { return m_queue_requests; } 
private:
    bs::PhysicalDeviceSelectInfo m_physical_device_select_info;
    bs::DeviceCreateInfo m_device_create_info;
    QueueRequestList m_queue_requests;   
};

} // namespace lcf::vkc
