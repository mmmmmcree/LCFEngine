#pragma once

#include <vulkan/vulkan.hpp>
#include <utility>
#include <vector>
#include "enums.h"
#include "resource_utils.h"

namespace lcf::vkc {

namespace details {

class CommandBufferAllocator;

} // namespace lcf::vkc::details

class CommandBufferProxy : public vk::CommandBuffer
{
    friend class details::CommandBufferAllocator;
    using Self = CommandBufferProxy;
    using Base = vk::CommandBuffer;
    using SemaphoreSubmitInfoList = std::vector<vk::SemaphoreSubmitInfo>;
    using ResourceLeaseList = std::vector<ResourceLease>;
    using ValidationData = const void *;
public:
    CommandBufferProxy(
        vk::CommandBuffer cmd_buffer,
        CommandBufferUsageFlags usage_flags,
        ValidationData validation_data 
    ) noexcept : 
        Base(cmd_buffer),
        m_usage_flags(usage_flags),
        m_validation_data(validation_data) {}
    CommandBufferProxy() = default;
    ~CommandBufferProxy() = default;
    CommandBufferProxy(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    CommandBufferProxy(Self && other) noexcept { this->stealFrom(other); }
    Self & operator=(Self && other) noexcept
    {
        if (this != &other) { this->stealFrom(other); }
        return *this;
    }
public:
    Self & pinLease(ResourceLease lease) noexcept
    {
        if (lease) { m_leases.emplace_back(std::move(lease)); }
        return *this;
    }
    Self & addWaitInfo(vk::SemaphoreSubmitInfo wait_info) noexcept
    {
        if (wait_info.semaphore) { m_wait_infos.emplace_back(wait_info); }
        return *this;
    }
    Self & addSignalInfo(vk::SemaphoreSubmitInfo signal_info) noexcept
    {
        if (signal_info.semaphore) { m_signal_infos.emplace_back(signal_info); }
        return *this;
    }
private:
    void stealFrom(Self & other) noexcept
    {
        Base::operator=(std::exchange(static_cast<vk::CommandBuffer &>(other), nullptr));
        m_leases = std::move(other.m_leases);
        m_wait_infos = std::move(other.m_wait_infos);
        m_signal_infos = std::move(other.m_signal_infos);
        m_usage_flags = std::exchange(other.m_usage_flags, {});
        m_validation_data = std::exchange(other.m_validation_data, nullptr);
    }
private:
    ResourceLeaseList m_leases;
    SemaphoreSubmitInfoList m_wait_infos;
    SemaphoreSubmitInfoList m_signal_infos;
    CommandBufferUsageFlags m_usage_flags = {};
    ValidationData m_validation_data = nullptr;
};

} // namespace lcf::vkc
