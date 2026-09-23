#pragma once

#include "containers/DoubleBuffer.h"
#include "event/Event.h"
#include "event/details/EventPacket.h"

#include <type_traits>
#include <utility>

namespace lcf::shader_toy {

class EventQueue
{
    using Self = EventQueue;
    using Buffer = DoubleBuffer<details::EventPacket>;
public:
    EventQueue() noexcept = default;
    EventQueue(const Self &) = delete;
    EventQueue(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    void push(details::EventPacket packet) noexcept { m_buffer.push(std::move(packet)); }
    template <typename E>
    requires event_c<std::remove_cvref_t<E>>
    void push(E && event) noexcept
    {
        this->push(details::make_event_packet(std::forward<E>(event)));
    }
    void publish() noexcept { m_buffer.publish(); }
    auto pollEvents() noexcept { return m_buffer.drain(); }
private:
    Buffer m_buffer;
};

} // namespace lcf::shader_toy
