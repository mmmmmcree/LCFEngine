#pragma once

#include <mutex>
#include <asio.hpp>
#include <asio/experimental/channel_error.hpp>
#include <expected>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <array>
#include <chrono>
#include <stdexcept>
#include <thread>

#include <async/task.h>

namespace lcf::async::testing {

using Context = asio::io_context;
using CancellationSignal = ::lcf::async::CancellationSource;

template <typename Completion>
auto bind_cancellation(CancellationSignal & source, Completion completion)
{
    return ::lcf::async::bind_cancellation(source, std::move(completion));
}
inline void cancel(CancellationSignal & source) { ::lcf::async::request_cancellation(source); }
inline std::error_code timer_cancelled_error() noexcept { return asio::error::operation_aborted; }
inline std::error_code wait_cancelled_error() noexcept { return asio::experimental::error::channel_cancelled; }

class Gate
{
public:
    Gate(Context & context, std::size_t) : m_signal(context.get_executor()) {}
    bool try_send(std::error_code) { return m_signal.set(); }
    auto receive() const { return m_signal.wait(); }
private:
    ::lcf::async::Signal m_signal;
};

template <typename Function>
void post(Context & context, Function function) { ::lcf::async::post(context.get_executor(), std::move(function)); }

inline lcf::async::Awaitable<void> yield() { return ::lcf::async::yield(); }
inline lcf::async::Awaitable<void> delay(std::chrono::steady_clock::duration duration) { return ::lcf::async::delay(duration); }
inline lcf::async::Awaitable<void> receive(Gate & gate) { co_await gate.receive(); }


class Runtime
{
    using Self = Runtime;
    using Guard = asio::executor_work_guard<asio::io_context::executor_type>;
    using WorkerList = std::array<std::jthread, 2>;
public:
    ~Runtime() noexcept
    {
        m_control.stop();
        m_cpu.stop();
        for (auto & worker : m_workers) {
            if (worker.joinable()) { worker.join(); }
        }
    }
    Runtime()
    {
        try {
            for (auto & worker : m_workers) { worker = std::jthread([this] { m_cpu.run(); }); }
        } catch (...) {
            m_cpu.stop();
            throw;
        }
    }
    Runtime(const Self &) = delete;
    Runtime(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    asio::any_io_executor control() { return m_control.get_executor(); }
    asio::any_io_executor cpu() { return m_cpu.get_executor(); }
    template <typename Predicate>
    void pumpUntil(Predicate predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {10};
        while (not predicate()) {
            m_control.poll_one();
            if (std::chrono::steady_clock::now() >= deadline) { throw std::runtime_error("async verification timed out"); }
            std::this_thread::yield();
        }
    }
    template <typename Task>
    void start(Task operation)
    {
        std::move(operation).start(m_control.get_executor(), [](const auto &) {});
    }
    template <typename Task>
    auto run(Task operation)
    {
        using Value = typename Task::value_type;
        struct Completion
        {
            task::ResultHandle<Value> m_result_rh;
            bool m_done = false;
        };
        auto completion_sp = std::make_shared<Completion>();
        auto token_sp = std::make_shared<typename Task::ResultToken>(operation.getResultToken());
        std::move(operation).start(m_control.get_executor(),
            [completion_sp, token_sp = std::move(token_sp)](const task::Result<Value> & result) mutable {
                completion_sp->m_result_rh = task::make_result_handle(std::move(token_sp), &result);
                completion_sp->m_done = true;
            });
        this->pumpUntil([&] { return completion_sp->m_done; });
        return completion_sp->m_result_rh;
    }
private:
    asio::io_context m_control;
    asio::io_context m_cpu;
    Guard m_control_guard = asio::make_work_guard(m_control);
    Guard m_cpu_guard = asio::make_work_guard(m_cpu);
    WorkerList m_workers;
};

}
