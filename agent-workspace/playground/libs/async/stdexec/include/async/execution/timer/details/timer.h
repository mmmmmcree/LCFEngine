#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <system_error>
#include <thread>

namespace lcf::async::execution::details {

struct TimerEntry
{
    std::chrono::steady_clock::time_point m_deadline;
    void (*m_finish)(std::shared_ptr<TimerEntry>, bool) noexcept;
    std::shared_ptr<TimerEntry> m_next_sp;
    bool m_decided = false;
};

class TimerService
{
    using EntryHandle = std::shared_ptr<TimerEntry>;
public:
    ~TimerService() noexcept
    {
        {
            std::lock_guard lock {m_mutex};
            m_stopping = true;
        }
        m_changed.notify_one();
        m_worker.join();
    }
    static TimerService & instance() { static TimerService service; return service; }
    void submit(EntryHandle entry_sp) noexcept
    {
        {
            std::lock_guard lock {m_mutex};
            if (entry_sp->m_decided) { return; }
            auto * position_p = &m_head_sp;
            while (*position_p and (*position_p)->m_deadline <= entry_sp->m_deadline) { position_p = &(*position_p)->m_next_sp; }
            entry_sp->m_next_sp = std::move(*position_p);
            *position_p = std::move(entry_sp);
        }
        m_changed.notify_one();
    }
    void cancel(EntryHandle entry_sp) noexcept
    {
        {
            std::lock_guard lock {m_mutex};
            if (entry_sp->m_decided) { return; }
            entry_sp->m_decided = true;
            auto * position_p = &m_head_sp;
            while (*position_p and position_p->get() != entry_sp.get()) { position_p = &(*position_p)->m_next_sp; }
            if (*position_p) { *position_p = std::move(entry_sp->m_next_sp); }
        }
        m_changed.notify_one();
        entry_sp->m_finish(entry_sp, true);
    }
private:
    TimerService() : m_worker([this] { this->run(); }) {}
    void run() noexcept
    {
        std::unique_lock lock {m_mutex};
        while (not m_stopping) {
            if (not m_head_sp) { m_changed.wait(lock); continue; }
            if (m_head_sp->m_deadline > std::chrono::steady_clock::now()) {
                const auto deadline = m_head_sp->m_deadline;
                m_changed.wait_until(lock, deadline);
                continue;
            }
            auto entry_sp = std::move(m_head_sp);
            m_head_sp = std::move(entry_sp->m_next_sp);
            entry_sp->m_decided = true;
            lock.unlock();
            entry_sp->m_finish(entry_sp, false);
            lock.lock();
        }
        while (m_head_sp) {
            auto entry_sp = std::move(m_head_sp);
            m_head_sp = std::move(entry_sp->m_next_sp);
            entry_sp->m_decided = true;
            lock.unlock();
            entry_sp->m_finish(entry_sp, true);
            lock.lock();
        }
    }
private:
    std::mutex m_mutex;
    std::condition_variable m_changed;
    EntryHandle m_head_sp;
    bool m_stopping = false;
    std::jthread m_worker;
};

}
