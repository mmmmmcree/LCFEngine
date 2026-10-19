#pragma once

#include <async/task.h>
#include <array>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace lcf::async::testing {

using CancellationSignal = ::lcf::async::CancellationSource;

template <typename Completion>
auto bind_cancellation(CancellationSignal & source, Completion completion)
{
    return ::lcf::async::bind_cancellation(source, std::move(completion));
}
inline void cancel(CancellationSignal & source) { ::lcf::async::request_cancellation(source); }
inline std::error_code timer_cancelled_error() noexcept { return std::make_error_code(std::errc::operation_canceled); }
inline std::error_code wait_cancelled_error() noexcept { return timer_cancelled_error(); }

class Context : public ::lcf::async::Context
{
public:
    lcf::async::Executor get_executor() const noexcept { return this->getExecutor(); }
    std::size_t poll_one() { return this->pollOne(); }
    std::size_t poll() { std::size_t count = 0; while (this->pollOne()) { ++count; } return count; }
    void run() { this->runUntilIdle(); }
    template <typename Rep, typename Period>
    void run_for(std::chrono::duration<Rep, Period> duration)
    {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < deadline) {
            if (not this->pollOne()) { std::this_thread::yield(); }
        }
    }
    void runForever() { ::lcf::async::Context::run(); }
};

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

inline auto yield() { return ::lcf::async::yield(); }
inline lcf::async::Awaitable<void> delay(std::chrono::steady_clock::duration duration) { co_await ::lcf::async::delay(duration); }
inline lcf::async::Awaitable<void> receive(Gate & gate) { co_await gate.receive(); }

class Runtime
{
    using Self = Runtime;
    using WorkerList = std::array<std::jthread, 2>;
public:
    ~Runtime() noexcept
    {
        m_control.stop();
        m_cpu.stop();
        for (auto & worker : m_workers) { if (worker.joinable()) { worker.join(); } }
    }
    Runtime()
    {
        try { for (auto & worker : m_workers) { worker = std::jthread([this] { m_cpu.runForever(); }); } }
        catch (...) { m_cpu.stop(); throw; }
    }
    Runtime(const Self &) = delete;
    Self & operator=(const Self &) = delete;
public:
    lcf::async::Executor control() { return m_control.getExecutor(); }
    lcf::async::Executor cpu() { return m_cpu.getExecutor(); }
    template <typename Predicate>
    void pumpUntil(Predicate predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {10};
        while (not predicate()) {
            m_control.pollOne();
            if (std::chrono::steady_clock::now() >= deadline) { throw std::runtime_error("async verification timed out"); }
            std::this_thread::yield();
        }
    }
    template <typename Task>
    void start(Task operation) { std::move(operation).start(this->control(), [](const auto &) {}); }
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
        std::move(operation).start(this->control(),
            [completion_sp, token_sp = std::move(token_sp)](const task::Result<Value> & result) mutable {
                completion_sp->m_result_rh = task::make_result_handle(std::move(token_sp), &result);
                completion_sp->m_done = true;
            });
        this->pumpUntil([&] { return completion_sp->m_done; });
        return completion_sp->m_result_rh;
    }
private:
    Context m_control;
    Context m_cpu;
    WorkerList m_workers;
};

}
