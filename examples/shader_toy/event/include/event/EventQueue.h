#pragma once

#include "event/Event.h"
#include "event/details/EventPacket.h"

#include <readerwriterqueue.h>
#include <exception>
#include <type_traits>
#include <utility>
#include <vector>

namespace lcf::shader_toy {

class EventQueue
{
    using Self = EventQueue;
    using Buffer = std::vector<details::EventPacket>;
public:
    EventQueue() noexcept = default;
    EventQueue(const Self &) = delete;
    EventQueue(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    // push and publish belong to one producer; pollEvents belongs to one consumer.
    void push(details::EventPacket packet) noexcept
    {
        m_pending.emplace_back(std::move(packet));
    }
    template <typename E>
    requires event_c<std::remove_cvref_t<E>>
    void push(E && event) noexcept
    {
        this->push(details::make_event_packet(std::forward<E>(event)));
    }
    void publish() noexcept
    {
        if (m_pending.empty()) { return; }
        if (not m_queue.enqueue(std::move(m_pending))) { std::terminate(); }
        m_pending.clear();
    }
    Buffer pollEvents() noexcept
    {
        Buffer batch;
        m_queue.try_dequeue(batch);
        return batch;
    }
private:
    Buffer m_pending;
    moodycamel::ReaderWriterQueue<Buffer> m_queue;
};

} // namespace lcf::shader_toy
