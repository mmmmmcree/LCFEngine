#pragma once

#include "concepts/invocable_concept.h"
#include <chrono>
#include <optional>
#include <functional>

namespace lcf {
    template <typename Invocable, crt_invocable_c<bool> ContinuePredicate>
    class PeriodicTask
    {
        using Self = PeriodicTask<Invocable, ContinuePredicate>;
    public:
        using Interval = typename std::chrono::steady_clock::duration;
        using CompleteCallbackOpt = std::optional<std::function<void()>>;
        PeriodicTask(
            Interval interval,
            Invocable invocable,
            ContinuePredicate continue_predicate,
            CompleteCallbackOpt complete_callback = std::nullopt) :
            m_interval(interval),
            m_invocable(std::move(invocable)),
            m_continue_predicate(std::move(continue_predicate)),
            m_complete_callback(std::move(complete_callback))
        {}
        ~PeriodicTask() = default;
        PeriodicTask(const Self &) = default;
        PeriodicTask(Self &&) = default;
        Self &operator=(const Self &) = default;
        Self &operator=(Self &&) = default;
        template <typename... Args> requires invocable_c<Invocable, Args...>
        auto operator()(Args &&... args) { return m_invocable(std::forward<Args>(args)...); }
        const Interval & getInterval() const noexcept { return m_interval; }
        bool isCompleted() const noexcept { return not m_continue_predicate(); }
        void onCompleted() { if (m_complete_callback) { (*m_complete_callback)(); } }
    private:
        Interval m_interval;
        Invocable m_invocable;
        ContinuePredicate m_continue_predicate;
        CompleteCallbackOpt m_complete_callback;
    };
}