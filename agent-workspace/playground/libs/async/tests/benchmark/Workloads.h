#pragma once

#include <Runtime.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace lcf::async::comparison {

namespace task = lcf::async::task;
using Clock = std::chrono::steady_clock;

enum class Workload { ePost, eChain, eFanout, eObservers, eTimerReady, eTimerWait, eInlineAwait, eYieldAwait, eWorkerChain };

struct Case
{
    std::string_view m_name;
    Workload m_workload;
    unsigned m_size;
};

inline constexpr Case k_cases[] {
    {"post", Workload::ePost, 256},
    {"chain", Workload::eChain, 64},
    {"fanout", Workload::eFanout, 256},
    {"observers", Workload::eObservers, 256},
    {"timer_ready", Workload::eTimerReady, 256},
    {"timer_wait", Workload::eTimerWait, 256},
    {"inline_await", Workload::eInlineAwait, 256},
    {"yield_await", Workload::eYieldAwait, 256},
    {"worker_chain", Workload::eWorkerChain, 64}
};

struct Samples
{
    std::vector<double> m_poll_us;
    std::vector<double> m_timer_lateness_us;
    std::uint64_t m_callbacks = 0;
    unsigned m_max_steps_per_poll = 0;
};

inline lcf::async::Awaitable<void> ready_step() { co_return; }

inline double percentile(std::vector<double> values, double fraction)
{
    if (values.empty()) { return 0; }
    std::sort(values.begin(), values.end());
    return values[static_cast<std::size_t>((values.size() - 1) * fraction)];
}

inline void run_case(Case test_case, Samples * samples_p = nullptr)
{
    testing::Context control;
    testing::Context worker;
    const auto control_executor = control.get_executor();
    std::atomic_uint steps = 0;
    std::atomic_bool stop = false;
    std::jthread worker_thread;
    if (test_case.m_workload == Workload::eWorkerChain) {
        worker_thread = std::jthread([&] {
            while (not stop.load(std::memory_order_acquire)) {
                worker.restart();
                if (worker.poll_one() == 0u) { std::this_thread::yield(); }
            }
        });
    }
    struct StopWorker
    {
        ~StopWorker() noexcept { m_stop.store(true, std::memory_order_release); }
        std::atomic_bool & m_stop;
    } stop_worker {stop};
    const auto task_executor = worker_thread.joinable() ? worker.get_executor() : control_executor;
    unsigned completions = 0;
    unsigned expected_completions = 1;
    const auto complete = [&](const auto & result) {
        if (not result) { throw std::system_error(result.error()); }
        ++completions;
    };
    if (test_case.m_workload == Workload::ePost) {
        expected_completions = test_case.m_size;
        for (unsigned index = 0; index < test_case.m_size; ++index) {
            testing::post(control, [&] { ++steps; ++completions; });
        }
    } else if (test_case.m_workload == Workload::eChain or test_case.m_workload == Workload::eWorkerChain) {
        auto chain = task::Chain {0u, task_executor};
        for (unsigned index = 0; index < test_case.m_size; ++index) {
            chain = std::move(chain).link([&](const unsigned & value) { ++steps; return value + 1; });
        }
        std::move(chain).start(control_executor, complete);
    } else if (test_case.m_workload == Workload::eFanout or test_case.m_workload == Workload::eObservers) {
        auto source = task::Chain {control_executor}.link([] { return 42; });
        const auto token = source.getResultToken();
        const auto junction = task::merge(token);
        expected_completions = test_case.m_size + 1;
        for (unsigned index = 0; index < test_case.m_size; ++index) {
            const auto & descriptor = test_case.m_workload == Workload::eObservers ? task::merge(token) : junction;
            auto branch = descriptor.link(control_executor, [&](const int & value) { ++steps; return value; });
            std::move(branch).start(control_executor, complete);
        }
        /* Let every branch suspend before its common source publishes. */
        while (control.poll_one()) {}
        control.restart();
        std::move(source).start(control_executor, complete);
    } else if (test_case.m_workload == Workload::eTimerReady or test_case.m_workload == Workload::eTimerWait) {
        expected_completions = test_case.m_size;
        const auto duration = test_case.m_workload == Workload::eTimerWait ? std::chrono::milliseconds {1} : std::chrono::milliseconds {0};
        for (unsigned index = 0; index < test_case.m_size; ++index) {
            auto chain = task::Chain {control_executor}.link([&, duration]() -> lcf::async::Awaitable<void> {
                const auto due = Clock::now() + duration;
                co_await testing::delay(duration);
                ++steps;
                if (samples_p) { samples_p->m_timer_lateness_us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - due).count()); }
            });
            std::move(chain).start(control_executor, complete);
        }
    } else {
        auto chain = task::Chain {control_executor}.link([&]() -> lcf::async::Awaitable<void> {
            for (unsigned index = 0; index < test_case.m_size; ++index) {
                if (test_case.m_workload == Workload::eYieldAwait) { co_await testing::yield(); }
                else { co_await ready_step(); }
                ++steps;
            }
        });
        std::move(chain).start(control_executor, complete);
    }
    const auto deadline = Clock::now() + std::chrono::seconds {30};
    while (completions < expected_completions) {
        control.restart();
        const auto before_steps = steps.load(std::memory_order_relaxed);
        const auto before = samples_p ? Clock::now() : Clock::time_point {};
        const auto count = control.poll_one();
        if (count and samples_p) {
            samples_p->m_poll_us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - before).count());
            samples_p->m_callbacks += count;
            samples_p->m_max_steps_per_poll = std::max(samples_p->m_max_steps_per_poll, steps.load(std::memory_order_relaxed) - before_steps);
        }
        if (count == 0u) { std::this_thread::yield(); }
        if (Clock::now() >= deadline) { throw std::runtime_error("backend comparison timed out"); }
    }
    while (control.poll_one()) {}
    if (steps != test_case.m_size) { throw std::runtime_error("incorrect comparison result"); }
}

}
