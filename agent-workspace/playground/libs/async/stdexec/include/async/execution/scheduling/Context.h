#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "Executor.h"

namespace lcf::async::execution {

LCF_ASYNC_EXPORT
struct PollBudget
{
    std::size_t m_max_callbacks = std::numeric_limits<std::size_t>::max();
    std::chrono::steady_clock::duration m_max_duration = std::chrono::steady_clock::duration::max();
};

LCF_ASYNC_EXPORT
struct PollReport
{
    std::size_t m_callbacks = 0;
    std::chrono::steady_clock::duration m_elapsed {};
    bool m_count_limit_reached = false;
    bool m_time_limit_reached = false;
};

LCF_ASYNC_EXPORT
class Context
{
    using Self = Context;
public:
    ~Context() noexcept { this->stop(); }
    Context() : m_queue_sp(std::make_shared<details::Queue>()), m_executor(QueueScheduler {m_queue_sp}) {}
    Context(const Self &) = delete;
    Self & operator=(const Self &) = delete;
public:
    Executor getExecutor() const noexcept { return m_executor; }
    QueueScheduler getScheduler() const noexcept { return QueueScheduler {m_queue_sp}; }
    std::size_t getReadyCount() const noexcept { return m_queue_sp->readyCount(); }
    std::size_t pollOne()
    {
        auto * callback_p = m_queue_sp->pop(false);
        if (not callback_p) { return 0; }
        callback_p->m_invoke(callback_p);
        return 1;
    }
    PollReport poll(PollBudget budget = {})
    {
        const auto started = std::chrono::steady_clock::now();
        const bool finite_duration = budget.m_max_duration != std::chrono::steady_clock::duration::max();
        PollReport report;
        while (report.m_callbacks < budget.m_max_callbacks) {
            if (finite_duration and std::chrono::steady_clock::now() - started >= budget.m_max_duration) { break; }
            if (not this->pollOne()) { break; }
            ++report.m_callbacks;
        }
        report.m_elapsed = std::chrono::steady_clock::now() - started;
        report.m_count_limit_reached = report.m_callbacks == budget.m_max_callbacks;
        report.m_time_limit_reached = finite_duration and report.m_elapsed >= budget.m_max_duration;
        return report;
    }
    void run()
    {
        while (auto * callback_p = m_queue_sp->pop(true)) { callback_p->m_invoke(callback_p); }
    }
    void runUntilIdle()
    {
        while (auto * callback_p = m_queue_sp->pop(true, true)) { callback_p->m_invoke(callback_p); }
    }
    void stop() noexcept { m_queue_sp->stop(); }
    void restart() noexcept { m_queue_sp->restart(); }
private:
    std::shared_ptr<details::Queue> m_queue_sp;
    Executor m_executor;
};

}
#endif
