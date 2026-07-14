#pragma once

#include <vulkan/vulkan.hpp>
#include <optional>
#include <mutex>

namespace lcf::vkc {

class LogicalQueue;

class QueueAccess
{
    using UniqueLock = std::unique_lock<std::mutex>;
    friend class LogicalQueue;
public:
    ~QueueAccess() noexcept = default;
    explicit QueueAccess(const LogicalQueue & logical_queue) noexcept;
    QueueAccess(const QueueAccess &) = delete;
    QueueAccess(QueueAccess &&) = default;
    QueueAccess &operator=(const QueueAccess &) = delete;
    QueueAccess &operator=(QueueAccess &&) = default;
    const vk::Queue * operator->() const noexcept { return &m_queue; }
private:
    vk::Queue m_queue = nullptr;
    UniqueLock m_lock;
};

} // namespace lcf::vkc