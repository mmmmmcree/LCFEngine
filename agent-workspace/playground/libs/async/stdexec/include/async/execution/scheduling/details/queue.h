#pragma once

#include <async/config.h>
#include <async/execution/stdexec.h>
#include <exec/any_sender_of.hpp>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <limits>
#include <utility>

namespace lcf::async::execution::details {

struct Callback
{
    void (*m_invoke)(Callback *);
    Callback * m_next_p = nullptr;
};

class Queue
{
public:
    void push(Callback * callback_p) noexcept
    {
        {
            std::lock_guard lock {m_mutex};
            if (m_tail_p) { m_tail_p->m_next_p = callback_p; }
            else { m_head_p = callback_p; }
            m_tail_p = callback_p;
            ++m_ready_count;
        }
        m_changed.notify_one();
    }
    Callback * pop(bool wait, bool until_idle = false)
    {
        std::unique_lock lock {m_mutex};
        if (wait) { m_changed.wait(lock, [&] { return m_head_p or m_stopped or (until_idle and m_work_count == 0); }); }
        if (m_stopped) { return nullptr; }
        auto * callback_p = m_head_p;
        if (callback_p) {
            m_head_p = callback_p->m_next_p;
            if (not m_head_p) { m_tail_p = nullptr; }
            callback_p->m_next_p = nullptr;
            --m_ready_count;
        }
        return callback_p;
    }
    void stop() noexcept
    {
        {
            std::lock_guard lock {m_mutex};
            m_stopped = true;
        }
        m_changed.notify_all();
    }
    void restart() noexcept { std::lock_guard lock {m_mutex}; m_stopped = false; }
    std::size_t readyCount() noexcept { std::lock_guard lock {m_mutex}; return m_ready_count; }
    void workStarted() noexcept { std::lock_guard lock {m_mutex}; ++m_work_count; }
    void workFinished() noexcept
    {
        { std::lock_guard lock {m_mutex}; --m_work_count; }
        m_changed.notify_all();
    }
private:
    std::mutex m_mutex;
    std::condition_variable m_changed;
    Callback * m_head_p = nullptr;
    Callback * m_tail_p = nullptr;
    bool m_stopped = false;
    std::size_t m_work_count = 0;
    std::size_t m_ready_count = 0;
};


}
