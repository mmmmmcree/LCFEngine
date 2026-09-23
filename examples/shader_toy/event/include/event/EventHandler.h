#pragma once

#include "event/Event.h"
#include "event/details/EventPacket.h"
#include "functional/UniqueFunction.h"
#include <concepts>
#include <functional>
#include <system_error>
#include <utility>

namespace lcf::shader_toy {

using EventHandler = UniqueFunction<std::error_code(const details::EventPacket &) noexcept>;
using EventErrorHandler = UniqueFunction<void(const details::EventPacket &) noexcept>;

struct ErrorCodeHash
{
    std::size_t operator()(const std::error_code & ec) const noexcept
    {
        return std::hash<int>{}(ec.value()) ^ (std::hash<const void *>{}(static_cast<const void *>(&ec.category())) << 1);
    }
};

namespace details {

template <event_c E>
struct EventHandlerGenerator
{
    template <typename F>
    requires std::is_nothrow_invocable_r_v<std::error_code, F &, const E &>
    EventHandler operator()(F && handler) const
    {
        return [handler = std::forward<F>(handler)](const details::EventPacket & packet) mutable noexcept -> std::error_code {
            return std::invoke(handler, details::event_as<E>(packet));
        };
    }
};

}

template <event_c E>
inline constexpr details::EventHandlerGenerator<E> make_event_handler {};

} // namespace lcf::shader_toy
