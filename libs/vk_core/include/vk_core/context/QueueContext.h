#pragma once

#include <vulkan/vulkan.hpp>
#include <queue>
#include <vector>
#include "vk_core/sync/TimelineSemaphore.h"
#include "vk_core/command/details/CommandBufferAllocator.h"
#include "resource_utils.h"

namespace lcf::vkc {

class CommandBufferProxy;

class QueueContext
{
    using Self = QueueContext;
    using LeaseBatch = std::pair<uint64_t, std::vector<ResourceLease>>;
    using LeaseBatchQueue = std::queue<LeaseBatch>;
public:
    ~QueueContext() noexcept = default;
    QueueContext() = default;
    QueueContext(const Self &) = delete;
    QueueContext(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    std::error_code create(vk::Device device, uint32_t family_index, uint32_t queue_index) noexcept;
    const vk::Queue & getQueue() const noexcept { return m_queue; }
    const uint32_t & getFamilyIndex() const noexcept { return m_family_index; }
    void submit(CommandBufferProxy && cmd_proxy) noexcept;
    void collectGarbage() noexcept;
private:
    vk::Device m_device;
    vk::Queue m_queue;
    uint32_t m_family_index = 0u;
    TimelineSemaphore m_timeline;
    details::CommandBufferAllocator m_cmd_allocator;
    LeaseBatchQueue m_lease_batch_queue;
};

} // namespace lcf::vkc
