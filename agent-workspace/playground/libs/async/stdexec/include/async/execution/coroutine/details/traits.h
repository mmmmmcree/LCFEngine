#pragma once

#include <async/execution/stdexec.h>

namespace lcf::async::execution::details {

template <typename T> struct AwaitablePromise;
template <typename T, typename ParentPromise> class TaskAwaiter;

template <typename Promise>
inline constexpr bool is_awaitable_promise_v = false;
template <typename T>
inline constexpr bool is_awaitable_promise_v<AwaitablePromise<T>> = true;

struct TaskAttributes
{
    static constexpr auto query(exec::get_completion_behavior_t<stde::set_value_t>) noexcept
    {
        return exec::completion_behavior::asynchronous_affine | exec::completion_behavior::inline_completion;
    }
};

}

