#pragma once

#include "event/Event.h"
#include "event/EventId.h"
#include "event/concepts.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace lcf::shader_toy::details {

struct EventPacket
{
    using Self = EventPacket;

    ~EventPacket() { this->reset(); }
    EventPacket() noexcept = default;
    EventPacket(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    EventPacket(Self && other) noexcept
        : m_id(other.m_id)
        , m_move_construct(other.m_move_construct)
        , m_destroy(other.m_destroy)
    {
        this->moveFrom(other);
    }
    Self & operator=(Self && other) noexcept
    {
        if (this == &other) { return *this; }
        this->reset();
        this->m_id = other.m_id;
        this->m_move_construct = other.m_move_construct;
        this->m_destroy = other.m_destroy;
        this->moveFrom(other);
        return *this;
    }
    explicit operator bool() const noexcept { return m_move_construct != nullptr; }
    void reset() noexcept
    {
        if (this->m_destroy) { this->m_destroy(this->m_storage); }
        this->m_destroy = nullptr;
        this->m_move_construct = nullptr;
        this->m_id = {};
    }
    void moveFrom(Self & other) noexcept
    {
        if (not other.m_move_construct) { return; }
        other.m_move_construct(this->m_storage, other.m_storage);
        other.m_destroy(other.m_storage);
        other.m_destroy = nullptr;
        other.m_move_construct = nullptr;
        other.m_id = {};
    }

    EventId m_id{};
    alignas(std::max_align_t) std::byte m_storage[k_max_event_size]{};
    void (*m_move_construct)(void * dst, void * src) noexcept = nullptr;
    void (*m_destroy)(void * p) noexcept = nullptr;
};

template <event_c E>
EventPacket make_event_packet(E && event) noexcept
{
    using T = std::remove_cvref_t<E>;
    EventPacket packet;
    packet.m_id = T::id_v;
    packet.m_move_construct = [](void * dst, void * src) noexcept {
        std::construct_at(static_cast<T *>(dst), std::move(*static_cast<T *>(src)));
    };
    packet.m_destroy = [](void * p) noexcept { std::destroy_at(static_cast<T *>(p)); };
    std::construct_at(reinterpret_cast<T *>(packet.m_storage), std::forward<E>(event));
    return packet;
}

template <event_c E>
E & event_as(EventPacket & packet) noexcept
{
    return *reinterpret_cast<E *>(packet.m_storage);
}

template <event_c E>
const E & event_as(const EventPacket & packet) noexcept
{
    return *reinterpret_cast<const E *>(packet.m_storage);
}

} // namespace lcf::shader_toy::details
