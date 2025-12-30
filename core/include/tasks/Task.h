#pragma once

#include "concepts/invocable_concept.h"

namespace lcf {
    template <typename Callable>
    class Task
    {
        using Self = Task<Callable>;
    public:
        Task(Callable invocable) : m_invocable(std::move(invocable)) {}
        ~Task() = default;
        Task(const Self &) = default;
        Task(Self &&) = default;
        Self &operator=(const Self &) = default;
        Self &operator=(Self &&) = default;
        template <typename... Args> requires invocable_c<Callable, Args...>
        auto operator()(Args &&... args) { return m_invocable(std::forward<Args>(args)...); }
    private:
        Callable m_invocable;
    };
}