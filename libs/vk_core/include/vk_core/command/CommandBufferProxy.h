#pragma once

#include <vulkan/vulkan.hpp>
#include <utility>
#include <vector>
#include <span>
#include <expected>
#include <system_error>
#include "enums.h"
#include "vk_core/error.h"
#include "resource_utils.h"

namespace lcf::vkc {

namespace details {

class CommandBufferAllocator;

} // namespace lcf::vkc::details

class CommandBufferBatch;

class CommandBufferProxy : public vk::CommandBuffer
{
    friend class details::CommandBufferAllocator;
    friend class CommandBufferBatch;
    using Self = CommandBufferProxy;
    using Base = vk::CommandBuffer;
    using SemaphoreSubmitInfoList = std::vector<vk::SemaphoreSubmitInfo>;
    using ResourceLeaseList = std::vector<ResourceLease>;
    using ValidationData = const void *;
public:
    ~CommandBufferProxy() = default;
    CommandBufferProxy(vk::CommandBuffer cmd_buffer, ValidationData validation_data) noexcept :
        Base(cmd_buffer), m_validation_data(validation_data) {}
    CommandBufferProxy(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    CommandBufferProxy(Self && other) noexcept { this->stealFrom(other); }
    Self & operator=(Self && other) noexcept { if (this != &other) { this->stealFrom(other); } return *this; }
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
        m_validation_data = std::exchange(other.m_validation_data, nullptr);
    }
private:
    ResourceLeaseList m_leases;
    SemaphoreSubmitInfoList m_wait_infos;
    SemaphoreSubmitInfoList m_signal_infos;
    ValidationData m_validation_data = nullptr;
};

class CommandBufferBatch
{
    friend class details::CommandBufferAllocator;
    using Self = CommandBufferBatch;
    using SemaphoreSubmitInfoList = std::vector<vk::SemaphoreSubmitInfo>;
    using ResourceLeaseList = std::vector<ResourceLease>;
    using CommandBufferList = std::vector<vk::CommandBuffer>;
    using ValidationData = const void *;
public:
    ~CommandBufferBatch() = default;
    CommandBufferBatch(
        std::span<vk::CommandBuffer> cmd_buffers,
        CommandBufferUsageFlags usage_flags,
        ValidationData validation_data 
    ) noexcept : 
        m_cmd_buffers(cmd_buffers.begin(), cmd_buffers.end()),
        m_usage_flags(usage_flags),
        m_validation_data(validation_data) {}
CommandBufferBatch(
        CommandBufferList cmd_buffers,
        CommandBufferUsageFlags usage_flags,
        ValidationData validation_data 
    ) noexcept : 
        m_cmd_buffers(std::move(cmd_buffers)),
        m_usage_flags(usage_flags),
        m_validation_data(validation_data) {}
    CommandBufferBatch(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    CommandBufferBatch(Self && other) noexcept { this->stealFrom(other); }
    Self & operator=(Self && other) noexcept { if (this != &other) { this->stealFrom(other); } return *this; }
public:
    std::expected<CommandBufferProxy, std::error_code> acquireProxy() noexcept
    {
        if (m_acquire_cursor >= m_cmd_buffers.size()) { return std::unexpected(make_error_code(errc::command_buffer_batch_exhausted)); }
        return CommandBufferProxy {m_cmd_buffers[m_acquire_cursor++], m_validation_data};
    }
    Self & collect(CommandBufferProxy && proxy) noexcept
    {
        if (proxy.m_validation_data != m_validation_data) { return *this; }
        m_leases.append_range(std::move(proxy.m_leases));
        m_wait_infos.append_range(std::move(proxy.m_wait_infos));
        m_signal_infos.append_range(std::move(proxy.m_signal_infos));
        return *this;
    }
    ValidationData getValidationData() const noexcept { return m_validation_data; }
    std::span<const vk::CommandBuffer> getCommandBuffers() const noexcept { return m_cmd_buffers; }
    std::span<const vk::SemaphoreSubmitInfo> getWaitInfos() const noexcept { return m_wait_infos; }
    std::span<const vk::SemaphoreSubmitInfo> getSignalInfos() const noexcept { return m_signal_infos; }
    ResourceLeaseList takeLeases() noexcept { return std::move(m_leases); }
private:
    void stealFrom(Self & other) noexcept
    {
        m_cmd_buffers = std::move(other.m_cmd_buffers);
        m_acquire_cursor = std::exchange(other.m_acquire_cursor, 0u);
        m_wait_infos = std::move(other.m_wait_infos);
        m_signal_infos = std::move(other.m_signal_infos);
        m_usage_flags = std::exchange(other.m_usage_flags, {});
        m_leases = std::move(other.m_leases);
        m_validation_data = std::exchange(other.m_validation_data, nullptr);
    }
private:
    uint32_t m_acquire_cursor = 0u;
    CommandBufferList m_cmd_buffers;
    SemaphoreSubmitInfoList m_wait_infos;
    SemaphoreSubmitInfoList m_signal_infos;
    ResourceLeaseList m_leases;
    CommandBufferUsageFlags m_usage_flags = {};
    ValidationData m_validation_data = nullptr;
};

} // namespace lcf::vkc
