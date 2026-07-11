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
    QueueAccess(const QueueAccess &) = delete;
    QueueAccess(QueueAccess &&) = default;
    QueueAccess &operator=(const QueueAccess &) = delete;
    QueueAccess &operator=(QueueAccess &&) = default;
private:
    QueueAccess(vk::Queue queue, std::optional<std::mutex> & mutex_opt) noexcept :
        m_queue(queue),
        m_lock(mutex_opt ? UniqueLock(mutex_opt.value()) : UniqueLock {})
    {
    }
public:
    const vk::Queue & getQueue() const noexcept { return m_queue; }
private:
    vk::Queue m_queue = nullptr;
    UniqueLock m_lock;
};

} // namespace lcf::vkc