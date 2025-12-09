#pragma once

#include <boost/asio.hpp>
#include "Task.h"
#include "PeriodicTask.h"
#include "Taskflow.h"

namespace lcf {
    namespace asio = boost::asio;

    class TaskScheduler
    {
        using Self = TaskScheduler;
    public:
        using IOContext = asio::io_context;
        TaskScheduler() = default;
        ~TaskScheduler() = default;
        TaskScheduler(const TaskScheduler &) = delete;
        TaskScheduler(TaskScheduler &&) = default;
        TaskScheduler & operator=(const TaskScheduler &) = delete;
        TaskScheduler & operator=(TaskScheduler &&) = default;
        void run() { m_io_context.run(); }
        void stop() { m_io_context.stop(); }
        IOContext & getIOContext() noexcept { return m_io_context; }
        const IOContext & getIOContext() const noexcept { return m_io_context; }
        template <typename Invocable, crt_invocable_c<bool> ContinuePredicate, typename... Args>
        Self & registerPeriodicTask(PeriodicTask<Invocable, ContinuePredicate> task, Args &&... args);
        Self & registerAwaitable(asio::awaitable<void> awaitable);
    private:
        static void rethrow_exception(std::exception_ptr execption_p);
        template <typename Invocable, crt_invocable_c<bool> ContinuePredicate, typename... Args>
        asio::awaitable<void> makeAwaitable(PeriodicTask<Invocable, ContinuePredicate> task, Args &&... args);
    private:
        IOContext m_io_context;
    };
}

template <typename Invocable, lcf::crt_invocable_c<bool> ContinuePredicate, typename... Args>
inline lcf::TaskScheduler & lcf::TaskScheduler::registerPeriodicTask(
    PeriodicTask<Invocable, ContinuePredicate> task,
    Args &&... args)
{
    asio::co_spawn(m_io_context,
        this->makeAwaitable(std::move(task), std::forward<Args>(args)...),
        &TaskScheduler::rethrow_exception);
    return *this;    
}

template <typename Invocable, lcf::crt_invocable_c<bool> ContinuePredicate, typename... Args>
inline boost::asio::awaitable<void> lcf::TaskScheduler::makeAwaitable(
    PeriodicTask<Invocable, ContinuePredicate> task,
    Args &&...args)
{
    using ClockTimer = asio::steady_timer;
    using Duration = typename ClockTimer::clock_type::duration;
    asio::steady_timer timer {m_io_context};
    while (not task.isCompleted()) {
        task(std::forward<Args>(args)...);
        timer.expires_after(task.getInterval());
        co_await timer.async_wait(asio::use_awaitable);
    }
    task.onCompleted();
    co_return;
}

inline lcf::TaskScheduler & lcf::TaskScheduler::registerAwaitable(asio::awaitable<void> awaitable)
{
    asio::co_spawn(m_io_context, std::move(awaitable), &TaskScheduler::rethrow_exception);
    return *this;
}