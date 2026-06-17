#pragma once

#include <vulkan/vulkan.hpp>
#include <array>
#include <memory>
#include <utility>
#include <vector>
#include "enums.h"
#include "enums/enum_count.h"

namespace lcf::vkc {

class DeviceContextCreateInfo;

class QueueContext;

class DeviceContext
{
    using Self = DeviceContext;
    using QueueContextUP = std::unique_ptr<QueueContext>;
public:
    DeviceContext() = default;
    ~DeviceContext() noexcept = default;
    DeviceContext(const Self &) = delete;
    DeviceContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    std::error_code create(vk::Instance instance, const DeviceContextCreateInfo & create_info) noexcept;
    const vk::PhysicalDevice & getPhysicalDevice() const noexcept { return m_physical_device; }
    const vk::Device & getDevice() const noexcept { return m_device; }
    const QueueContext & getQueueContext(enums::QueueRole role) const noexcept { return *m_queue_context_table[std::to_underlying(role)]; }
private:
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
    std::vector<QueueContextUP> m_queue_contexts;
    std::array<QueueContext *, enum_count_v<enums::QueueRole>> m_queue_context_table = {};
};


} // namespace lcf::vkc
