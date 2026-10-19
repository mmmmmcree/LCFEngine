#pragma once

#include "../Awaitable.h"
#include "../FrameAllocatorScope.h"

namespace lcf::async::execution {

namespace details {

template <typename T>
struct TaskValue
{
    static constexpr std::size_t k_alignment = alignof(T);
#if defined(__clang__) and defined(_MSC_VER)
    // MSVC's coroutine_handle passes zero promise alignment to Clang's builtin.
    static constexpr bool k_indirect_value = alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
    static constexpr bool k_indirect_value = false;
#endif
    using Storage = std::conditional_t<k_indirect_value, std::unique_ptr<T>, std::optional<T>>;
    void return_value(T value) noexcept(not k_indirect_value and std::is_nothrow_move_constructible_v<T>)
    {
        if constexpr (k_indirect_value) { m_value = std::make_unique<T>(std::move(value)); }
        else { m_value.emplace(std::move(value)); }
    }
    T takeValue() noexcept(std::is_nothrow_move_constructible_v<T>) { return std::move(*m_value); }
    Storage m_value;
};

template <>
struct TaskValue<void>
{
    static constexpr std::size_t k_alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
    void return_void() noexcept {}
    void takeValue() noexcept {}
};

template <typename Scheduler, typename Promise>
class RescheduleAwaiter
{
    using Self = RescheduleAwaiter;
    using Handle = std::coroutine_handle<Promise>;
    struct Receiver
    {
        using receiver_concept = stde::receiver_t;
        auto get_env() const noexcept { return stde::get_env(m_handle.promise()); }
        void set_value() noexcept { m_handle.resume(); }
        void set_error(std::exception_ptr exception) noexcept
        {
            m_awaiter_p->m_exception = std::move(exception);
            m_handle.resume();
        }
        void set_stopped() noexcept { m_handle.promise().unhandled_stopped().resume(); }
        Self * m_awaiter_p;
        Handle m_handle;
    };
    using Sender = decltype(stde::unstoppable(stde::schedule(std::declval<Scheduler>())));
    using Operation = stde::connect_result_t<Sender, Receiver>;
    struct Holder
    {
        Holder(Scheduler scheduler, Self * awaiter_p, Handle handle) :
            m_operation(stde::connect(stde::unstoppable(stde::schedule(std::move(scheduler))), Receiver {awaiter_p, handle})) {}
        Operation m_operation;
    };
public:
    explicit RescheduleAwaiter(Scheduler scheduler) noexcept : m_scheduler(std::move(scheduler)) {}
public:
    bool await_ready() const noexcept { return false; }
    bool await_suspend(Handle handle) noexcept
    {
        try {
            auto & promise = handle.promise();
            if (not promise.m_rescheduled) {
                auto cleanup = exec::at_coroutine_exit([](auto scheduler) {
                    return stde::unstoppable(stde::schedule(std::move(scheduler)));
                }, stde::get_start_scheduler(promise.get_env()));
                cleanup.await_suspend(handle);
                (void)cleanup.await_resume();
                promise.m_rescheduled = true;
            }
            promise.getMutableContext().set_scheduler(m_scheduler);
            m_holder_opt.emplace(std::move(m_scheduler), this, handle);
            stde::start(m_holder_opt->m_operation);
            return true;
        } catch (...) {
            m_exception = std::current_exception();
            return false;
        }
    }
    void await_resume() { if (m_exception) { std::rethrow_exception(std::move(m_exception)); } }
private:
    Scheduler m_scheduler;
    std::exception_ptr m_exception;
    std::optional<Holder> m_holder_opt;
};

template <typename T>
struct AwaitablePromise : TaskValue<T>, stde::with_awaitable_senders<AwaitablePromise<T>>
{
    using Self = AwaitablePromise;
    using Base = stde::with_awaitable_senders<Self>;
    using Context = typename exec::default_task_context<T>::template promise_context_t<Self>;
    struct FinalAwaiter
    {
        bool await_ready() const noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Self> handle) const noexcept
        {
            return handle.promise().continuation().handle();
        }
        void await_resume() const noexcept {}
    };
public:
    static void * operator new(std::size_t size) { return allocate_frame(size, std::max(alignof(Self), TaskValue<T>::k_alignment)); }
    static void operator delete(void * frame_p, std::size_t size) noexcept
    {
        deallocate_frame(frame_p, size, std::max(alignof(Self), TaskValue<T>::k_alignment));
    }
    Awaitable<T> get_return_object() noexcept { return Awaitable<T> {std::coroutine_handle<Self>::from_promise(*this)}; }
    std::suspend_always initial_suspend() const noexcept { return {}; }
    FinalAwaiter final_suspend() const noexcept { return {}; }
    void unhandled_exception() noexcept { m_exception = std::current_exception(); }
    const Context & get_env() const noexcept { return *m_context_p; }
    Context & getMutableContext() noexcept(std::is_nothrow_copy_constructible_v<Context>)
    {
        if (not m_context_opt) {
            m_context_opt.emplace(*m_context_p);
            m_context_p = &*m_context_opt;
        }
        return *m_context_opt;
    }
    template <typename Value>
    auto await_transform(Awaitable<Value> task) noexcept { return std::move(task).as_awaitable(*this); }
    template <stde::sender Sender>
    decltype(auto) await_transform(Sender && sender)
    {
        using Behavior = decltype(exec::get_completion_behavior_t<stde::set_value_t> {}(stde::get_env(sender), this->get_env()));
        if constexpr (exec::completion_behavior::is_affine(Behavior {})) {
            return stde::as_awaitable(std::forward<Sender>(sender), *this);
        } else {
            return stde::as_awaitable(stde::continues_on(std::forward<Sender>(sender), stde::get_start_scheduler(this->get_env())), *this);
        }
    }
    template <template <typename> class Request, stde::scheduler Scheduler>
    requires std::same_as<Request<Scheduler>, decltype(exec::reschedule_coroutine_on(std::declval<Scheduler>()))>
    auto await_transform(Request<Scheduler> request) noexcept
    {
        auto [scheduler] = std::move(request);
        return RescheduleAwaiter<Scheduler, Self> {std::move(scheduler)};
    }
    template <typename Value>
    decltype(auto) await_transform(Value && value) { return Base::await_transform(std::forward<Value>(value)); }
public:
    std::exception_ptr m_exception;
    std::optional<Context> m_context_opt;
    const Context * m_context_p {nullptr};
    bool m_rescheduled {false};
};

template <typename T, typename ParentPromise>
class TaskAwaiter
{
    using Self = TaskAwaiter;
    using Promise = AwaitablePromise<T>;
    using Handle = std::coroutine_handle<Promise>;
    using Context = exec::awaiter_context_t<Promise, ParentPromise>;
public:
    ~TaskAwaiter() noexcept { if (m_handle) { m_handle.destroy(); } }
    explicit TaskAwaiter(Handle handle) noexcept : m_handle(handle) {}
    TaskAwaiter(const Self &) = delete;
    TaskAwaiter(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    bool await_ready() const noexcept { return false; }
    template <typename Parent>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Parent> parent) noexcept
    {
        auto & promise = m_handle.promise();
        if constexpr (is_awaitable_promise_v<Parent>) {
            // The awaiting parent keeps this context alive; scheduler changes detach it.
            promise.m_context_p = &parent.promise().get_env();
        } else {
            promise.m_context_opt.emplace(parent.promise());
            promise.m_context_p = &*promise.m_context_opt;
            m_context_opt.emplace(*promise.m_context_opt, parent.promise());
        }
        promise.set_continuation(parent);
        return m_handle;
    }
    T await_resume()
    {
        m_context_opt.reset();
        struct FrameOwner
        {
            ~FrameOwner() noexcept { m_handle.destroy(); }
            Handle m_handle;
        } owner {std::exchange(m_handle, {})};
        auto & promise = owner.m_handle.promise();
        if (promise.m_exception) { std::rethrow_exception(std::move(promise.m_exception)); }
        return promise.takeValue();
    }
private:
    Handle m_handle;
    std::optional<Context> m_context_opt;
};

}

}
