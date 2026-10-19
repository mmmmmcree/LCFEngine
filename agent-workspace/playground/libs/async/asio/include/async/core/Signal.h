#pragma once

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "Awaitable.h"
#include "Executor.h"
#include <asio/experimental/concurrent_channel.hpp>
#include <asio/redirect_error.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <system_error>
#include <utility>

namespace lcf::async {

class Signal
{
    using Self = Signal;
    using Channel = asio::experimental::concurrent_channel<void(std::error_code)>;
    using ChannelHandle = std::shared_ptr<Channel>;
    class State
    {
    public:
        explicit State(std::pmr::memory_resource * resource_p) noexcept : m_resource_p(resource_p) {}
    public:
        ChannelHandle subscribe(Executor executor)
        {
            std::lock_guard lock {m_mutex};
            if (m_set) { return {}; }
            if (not m_channel_sp) { m_channel_sp = this->makeChannel(std::move(executor)); }
            return m_channel_sp;
        }
        bool set()
        {
            ChannelHandle channel_sp;
            {
                std::lock_guard lock {m_mutex};
                if (m_set) { return false; }
                m_set = true;
                channel_sp = std::move(m_channel_sp);
            }
            if (channel_sp) { channel_sp->close(); }
            return true;
        }
    private:
        ChannelHandle makeChannel(Executor executor)
        {
            return std::allocate_shared<Channel>(std::pmr::polymorphic_allocator<Channel> {m_resource_p}, std::move(executor), 0u);
        }
    private:
        std::mutex m_mutex;
        ChannelHandle m_channel_sp;
        std::pmr::memory_resource * m_resource_p;
        bool m_set = false;
    };
    using StateHandle = std::shared_ptr<State>;
public:
    explicit Signal(std::pmr::memory_resource * resource_p = std::pmr::get_default_resource()) :
        m_state_sp(std::allocate_shared<State>(std::pmr::polymorphic_allocator<State> {resource_p}, resource_p)) {}
    Signal(Executor executor, std::pmr::memory_resource * resource_p = std::pmr::get_default_resource()) : Signal(resource_p)
    {
        m_state_sp->subscribe(std::move(executor));
    }
    Signal(const Self &) = delete;
    Self & operator=(const Self &) = delete;
public:
    bool set() { return m_state_sp->set(); }
    Awaitable<void> wait() const { return Self::wait_state(m_state_sp); }
private:
    static Awaitable<void> wait_state(StateHandle state_sp)
    {
        auto channel_sp = state_sp->subscribe(co_await asio::this_coro::executor);
        if (not channel_sp) { co_return; }
        std::error_code error;
        co_await channel_sp->async_receive(asio::redirect_error(asio::use_awaitable, error));
        if (error and error != asio::experimental::error::channel_closed) { throw std::system_error(error); }
    }
private:
    StateHandle m_state_sp;
};

}
#endif
