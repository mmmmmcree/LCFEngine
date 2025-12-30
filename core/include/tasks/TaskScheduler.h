#pragma once

#include "IOContext.h"
#include "Task.h"
#include "PeriodicTask.h"
#include "Taskflow.h"
#include <memory>

namespace lcf {
    class TaskScheduler
    {
        using Self = TaskScheduler;
    public:
        enum class RunMode
        {
            eThisThread,
            eNewThread
        };
    public:
        explicit TaskScheduler(RunMode run_mode);
        ~TaskScheduler() = default;
        TaskScheduler(const TaskScheduler &) = delete;
        TaskScheduler(TaskScheduler &&) = default;
        TaskScheduler & operator=(const TaskScheduler &) = delete;
        TaskScheduler & operator=(TaskScheduler &&) = default;
        void run() { m_io_context_up->run(); }
        void stop() { m_io_context_up->stop(); }
        IOContext & getIOContext() noexcept { return *m_io_context_up; }
        const IOContext & getIOContext() const noexcept { return *m_io_context_up; }
        template <callable_c Callable, crt_invocable_c<bool> ContinuePredicate, typename... Args>
        Self & registerPeriodicTask(PeriodicTask<Callable, ContinuePredicate> task, Args &&... args);
        Self & registerAwaitable(asio::awaitable<void> awaitable);
        template <invocable_c Callable>        
        void post(Callable && invocable);
    private:
        static void rethrow_exception(std::exception_ptr execption_p);
        template <invocable_c Callable, crt_invocable_c<bool> ContinuePredicate, typename... Args>
        asio::awaitable<void> makeAwaitable(PeriodicTask<Callable, ContinuePredicate> task, Args &&... args);
    private:
        std::unique_ptr<IOContext> m_io_context_up;
    };
}

template <lcf::callable_c Callable, lcf::crt_invocable_c<bool> ContinuePredicate, typename... Args>
inline lcf::TaskScheduler & lcf::TaskScheduler::registerPeriodicTask(
    PeriodicTask<Callable, ContinuePredicate> task,
    Args &&... args)
{
    asio::co_spawn(this->getIOContext(),
        this->makeAwaitable(std::move(task), std::forward<Args>(args)...),
        &TaskScheduler::rethrow_exception);
    return *this;    
}

template <lcf::invocable_c Callable>
inline void lcf::TaskScheduler::post(Callable &&invocable)
{
    asio::post(this->getIOContext(), std::forward<Callable>(invocable));
}

template <lcf::invocable_c Callable, lcf::crt_invocable_c<bool> ContinuePredicate, typename... Args>
inline boost::asio::awaitable<void> lcf::TaskScheduler::makeAwaitable(
    PeriodicTask<Callable, ContinuePredicate> task,
    Args &&...args)
{
    using ClockTimer = asio::steady_timer;
    using Duration = typename ClockTimer::clock_type::duration;
    asio::steady_timer timer {this->getIOContext()};
    while (not task.isCompleted()) {
        task(std::forward<Args>(args)...);
        timer.expires_after(task.getInterval());
        co_await timer.async_wait(asio::use_awaitable);
    }
    task.onCompleted();
    co_return;
}

inline lcf::TaskScheduler::TaskScheduler(RunMode run_mode)
{
    switch (run_mode) {
        case RunMode::eThisThread: { m_io_context_up = std::make_unique<IOContext>(); } break;
        case RunMode::eNewThread: { m_io_context_up = std::make_unique<ThreadIOContext>(); } break;
    }
}

inline lcf::TaskScheduler &lcf::TaskScheduler::registerAwaitable(asio::awaitable<void> awaitable)
{
    asio::co_spawn(this->getIOContext(), std::move(awaitable), &TaskScheduler::rethrow_exception);
    return *this;
}