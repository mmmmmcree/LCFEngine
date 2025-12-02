#pragma once

#include "concepts/invocable_concept.h"
#include <boost/asio.hpp>
#include <taskflow/taskflow.hpp>
#include <optional>

namespace asio = boost::asio;

namespace lcf {
class TaskScheduler
    {
    public:
        using IOContext = asio::io_context;
        using ClockTimer = asio::steady_timer;
        using Interval = typename ClockTimer::clock_type::duration;
        using ExceptionHandler = std::function<void(std::exception_ptr)>;
        using Taskflow = tf::Taskflow;

        explicit TaskScheduler(size_t executor_threads = std::thread::hardware_concurrency()) :
            m_executor(executor_threads)
        {

        }
        IOContext & getIOContext() noexcept { return m_io_context; }
        const IOContext & getIOContext() const noexcept { return m_io_context; }

        void run() { m_io_context.run(); }

        template <crt_invocable_c<void> Task, crt_invocable_c<bool> ContinueCondition>
        void registerPeriodicTask(
            Interval interval,
            Task && task,
            ContinueCondition && continue_condition,
            std::optional<ExceptionHandler> error_handler_opt = std::nullopt)
        {
            auto awaitable_task = this->wrapPeriodicTask(
                interval,
                std::forward<Task>(task),
                std::forward<ContinueCondition>(continue_condition));
            
            auto callback = [error_handler_opt = std::move(error_handler_opt)] (std::exception_ptr e) {
                handle_exception(e, error_handler_opt);
            };
            asio::co_spawn(m_io_context, std::move(awaitable_task), std::move(callback));
        }

        template <typename AsyncTaskGetter,
                typename TaskflowGetter,
                typename T = typename std::invoke_result_t<AsyncTaskGetter>::value_type> requires
                crt_invocable_c<AsyncTaskGetter, asio::awaitable<T>> &&
                crt_invocable_c<TaskflowGetter, Taskflow, T>
        void registerAsyncTask(
            AsyncTaskGetter async_task_getter,
            TaskflowGetter taskflow_getter,
            std::optional<ExceptionHandler> error_handler_opt = std::nullopt)
        {
            auto awaitable = [this, async_task_getter = std::move(async_task_getter), taskflow_getter = std::move(taskflow_getter)]() -> asio::awaitable<void>
            {
                T result = co_await async_task_getter();
                auto executor = co_await asio::this_coro::executor;
                co_await this->wrapTaskflowAsync(taskflow_getter(std::move(result)), std::move(executor));
                co_return;
            };
            auto callback = [&error_handler_opt] (std::exception_ptr e) {
                handle_exception(e, error_handler_opt);
            };
            asio::co_spawn(m_io_context, std::move(awaitable), std::move(callback));
        }
    private:
        static void handle_exception(std::exception_ptr exception_p, std::optional<ExceptionHandler> error_handler_opt);


        template <crt_invocable_c<void> Task, crt_invocable_c<bool> ContinueCondition>
        asio::awaitable<void> wrapPeriodicTask(
            Interval interval,
            Task && task,
            ContinueCondition && continue_condition) noexcept
        {
            ClockTimer timer(m_io_context);
            while (continue_condition()) {
                task();
                timer.expires_after(interval);
                co_await timer.async_wait(asio::use_awaitable);
            }
            co_return;
        }

        asio::awaitable<void> wrapTaskflowAsync(Taskflow taskflow, asio::any_io_executor io_executor)
        {
            auto taskflow_sp = std::make_shared<Taskflow>(std::move(taskflow));
            auto async_task = [this, taskflow_sp, io_executor = std::move(io_executor)](auto handler) {
                auto handler_sp = std::make_shared<std::decay_t<decltype(handler)>>(std::move(handler));
                auto notify = taskflow_sp->emplace([handler_sp, io_executor = std::move(io_executor)]() {
                    asio::post(io_executor, [handler_sp]() { (*handler_sp)(); });
                });
                taskflow_sp->for_each_task([&](tf::Task t) {
                    if (t.num_successors() > 0 or t == notify) { return; }
                    t.precede(notify);
                });
                m_executor.silent_async([this, taskflow_sp]() {
                    m_executor.run(*taskflow_sp);
                });
            };
            return asio::async_initiate<void()>(std::move(async_task), asio::use_awaitable);
        }

    private:
        IOContext m_io_context;
        tf::Executor m_executor;
    };
}