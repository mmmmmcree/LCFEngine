#pragma once

#include "concepts/invocable_concept.h"

namespace lcf {
    template <typename Invocable>
    class Task
    {
        using Self = Task<Invocable>;
    public:
        Task(Invocable invocable) : m_invocable(std::move(invocable)) {}
        ~Task() = default;
        Task(const Self &) = default;
        Task(Self &&) = default;
        Self &operator=(const Self &) = default;
        Self &operator=(Self &&) = default;
        template <typename... Args> requires invocable_c<Invocable, Args...>
        auto operator()(Args &&... args) { return m_invocable(std::forward<Args>(args)...); }
    private:
        Invocable m_invocable;
    };
}