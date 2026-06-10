#pragma once

#include "vk_core/context/QueueContext.h"
#include "vk_core/context/enums.h"
#include "enums/enum_count.h"
#include <array>
#include <span>
#include <utility>
#include <vector>

namespace lcf::vkc {

class DeviceContext
{
    using Self = DeviceContext;
public:
    DeviceContext() = default;
    ~DeviceContext() noexcept = default;
    DeviceContext(const Self &) = delete;
    DeviceContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    const vk::PhysicalDevice & getPhysicalDevice() const noexcept { return m_physical_device; }
    const vk::Device & getDevice() const noexcept { return m_device; }
    std::span<QueueContext> queueContexts() noexcept { return m_queue_contexts; }
    QueueContext & getQueueContext(enums::QueueRole role) noexcept
    {
        return *m_queue_table[std::to_underlying(role)];
    }

private:
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
    // capability snapshot, device-level dispatch, VMA allocator
    std::vector<QueueContext> m_queue_contexts; // ownership: every queue of every family
    // role index built by bootstrap's collapse ladder: dedicated family first, spare
    // same-family queue second, eMainGraphics last; several roles may alias one queue
    // on scarce devices — safe because QueueContext is the single submission funnel
    std::array<QueueContext *, enum_count_v<enums::QueueRole>> m_queue_table {};
};

} // namespace lcf::vkc
