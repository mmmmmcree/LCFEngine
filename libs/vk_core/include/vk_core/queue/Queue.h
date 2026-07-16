#pragma once

#include <vulkan/vulkan.hpp>
#include <queue>
#include <expected>
#include <queue>
#include "vk_core/queue/LogicalQueue.h"
#include "vk_core/queue/SubmissionToken.h"
#include "vk_core/sync/TimelineSemaphore.h"
#include "vk_core/command/CommandBufferAllocator.h"
#include "resource_utils.h"

namespace lcf::vkc {

class CommandBufferBatch;
class CommandBufferAllocateInfo;

class Queue
{
    using Self = Queue;
    using LeaseBatch = std::pair<uint64_t, std::vector<ResourceLease>>;
    using LeaseBatchQueue = std::queue<LeaseBatch>;
public:
    ~Queue() noexcept = default;
    Queue() = default;
    Queue(const Self &) = delete;
    Queue(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    std::error_code create(const LogicalQueue & logical_queue) noexcept;
    std::expected<CommandBufferBatch, std::error_code> allocateCommandBufferBatch(const CommandBufferAllocateInfo & info) noexcept;
    std::expected<SubmissionToken, std::error_code> submit(CommandBufferBatch && batch) noexcept;
    void collectGarbage() noexcept;
private:
    LogicalQueue m_logical_queue;
    TimelineSemaphore m_timeline;
    details::CommandBufferAllocator m_cmd_allocator;
    LeaseBatchQueue m_lease_batch_queue;
};

} // namespace lcf::vkc