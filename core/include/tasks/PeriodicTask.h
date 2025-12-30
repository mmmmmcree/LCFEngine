#pragma once

#include "concepts/invocable_concept.h"
#include <chrono>
#include <optional>
#include <functional>

namespace lcf {
    template <callable_c Callable, crt_invocable_c<bool> ContinuePredicate>
    class PeriodicTask
    {
        using Self = PeriodicTask<Callable, ContinuePredicate>;
    public:
        using Interval = typename std::chrono::steady_clock::duration;
        using CompleteCallbackOpt = std::optional<std::function<void()>>;
        PeriodicTask(
            Interval interval,
            Callable callable,
            ContinuePredicate continue_predicate,
            CompleteCallbackOpt complete_callback = std::nullopt) :
            m_interval(interval),
            m_callable(std::move(callable)),
            m_continue_predicate(std::move(continue_predicate)),
            m_complete_callback(std::move(complete_callback))
        {}
        ~PeriodicTask() = default;
        PeriodicTask(const Self &) = default;
        PeriodicTask(Self &&) = default;
        Self &operator=(const Self &) = default;
        Self &operator=(Self &&) = default;
        template <typename... Args> requires invocable_c<Callable, Args...>
        auto operator()(Args &&... args) { return m_callable(std::forward<Args>(args)...); }
        const Interval & getInterval() const noexcept { return m_interval; }
        bool isCompleted() const noexcept { return not m_continue_predicate(); }
        void onCompleted() { if (m_complete_callback) { (*m_complete_callback)(); } }
    private:
        Interval m_interval;
        Callable m_callable;
        ContinuePredicate m_continue_predicate;
        CompleteCallbackOpt m_complete_callback;
    };
}