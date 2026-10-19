#pragma once

#include "common.h"
#include "notification.h"
#include <atomic>

namespace lcf::async::task::details::chain {

template <result_value_c T>
struct ExecutionState
{
    using Self = ExecutionState;
    using Operation = std::move_only_function<Awaitable<void>(Self &, Executor)>;
public:
    ExecutionState() noexcept = default;
    explicit ExecutionState(Operation operation) noexcept : m_operation(std::move(operation)) {}
public:
    void publish(Result<T> result) noexcept(std::is_nothrow_move_constructible_v<Result<T>>)
    {
        m_result_placeholder.emplace(std::move(result));
        m_ready.store(true, std::memory_order_release);
        m_publication.complete();
    }
    void publish(std::unexpected<std::error_code> failure) noexcept
    {
        m_result_placeholder.emplace(std::unexpect, failure.error());
        m_ready.store(true, std::memory_order_release);
        m_publication.complete();
    }
    bool isReady() const noexcept { return m_ready.load(std::memory_order_acquire); }
    const Result<T> & result() const noexcept { return *m_result_placeholder; }
public:
    Operation m_operation;
    details::Notification m_publication {1};
private:
    std::optional<Result<T>> m_result_placeholder;
    std::atomic_bool m_ready = false;
};

template <result_value_c T>
using ExecutionStateHandle = std::shared_ptr<ExecutionState<T>>;

template <result_value_c T>
class ExecutionSender
{
    using StateHandle = ExecutionStateHandle<T>;
    struct Environment
    {
        static constexpr auto query(exec::get_completion_behavior_t<stde::set_value_t>) noexcept
        {
            return exec::completion_behavior::asynchronous_affine | exec::completion_behavior::inline_completion;
        }
    };
    template <typename Receiver>
    class Operation final : public ::lcf::async::execution::details::Callback
    {
        using Self = Operation;
        using Function = typename ExecutionState<T>::Operation;
        struct EntryReceiver
        {
            using receiver_concept = stde::receiver_t;
            stde::env_of_t<Receiver> get_env() const noexcept { return stde::get_env(m_operation_p->m_receiver); }
            void set_value() noexcept { m_operation_p->run(); }
            void set_error(std::exception_ptr exception) noexcept { m_operation_p->enqueueFailure(exception_error(std::move(exception))); }
            void set_stopped() noexcept { m_operation_p->enqueueFailure(std::make_error_code(std::errc::operation_canceled)); }
            Self * m_operation_p;
        };
        struct TaskReceiver
        {
            using receiver_concept = stde::receiver_t;
            stde::env_of_t<Receiver> get_env() const noexcept { return stde::get_env(m_operation_p->m_receiver); }
            void set_value() noexcept { m_operation_p->complete(); }
            void set_error(std::exception_ptr exception) noexcept { m_operation_p->fail(exception_error(std::move(exception))); }
            void set_stopped() noexcept { m_operation_p->fail(std::make_error_code(std::errc::operation_canceled)); }
            Self * m_operation_p;
        };
        using EntryOperation = stde::connect_result_t<Executor::ScheduleSender, EntryReceiver>;
        struct EntryHolder
        {
            EntryHolder(Executor executor, Self * operation_p) : m_operation(stde::connect(stde::schedule(executor), EntryReceiver {operation_p})) {}
            EntryOperation m_operation;
        };
        struct Holder
        {
            using TaskOperation = stde::connect_result_t<Awaitable<void>, TaskReceiver>;
            Holder(Awaitable<void> task, Self * operation_p) : m_operation(stde::connect(std::move(task), TaskReceiver {operation_p})) {}
            TaskOperation m_operation;
        };
    public:
        using operation_state_concept = stde::operation_state_t;
    public:
        Operation(StateHandle state_handle, Executor executor, Receiver receiver) :
            ::lcf::async::execution::details::Callback {&Self::deliver_failure}, m_state_handle(std::move(state_handle)),
            m_executor(std::move(executor)), m_receiver(std::move(receiver)),
            m_function(std::exchange(m_state_handle->m_operation, {})) {}
        Operation(const Self &) = delete;
        Operation(Self &&) = delete;
        Self & operator=(const Self &) = delete;
        Self & operator=(Self &&) = delete;
    public:
        void start() noexcept
        {
            if (m_state_handle->isReady()) { this->complete(); return; }
            try {
                m_entry_opt.emplace(m_executor, this);
                stde::start(m_entry_opt->m_operation);
            } catch (...) { this->enqueueFailure(exception_error(std::current_exception())); }
        }
    private:
        void run() noexcept
        {
            try {
                m_holder_opt.emplace(m_function(*m_state_handle, m_executor), this);
                stde::start(m_holder_opt->m_operation);
            } catch (...) { this->fail(exception_error(std::current_exception())); }
        }
        void enqueueFailure(std::error_code error) noexcept
        {
            m_failure = error;
            m_executor.enqueue(this);
        }
        static void deliver_failure(::lcf::async::execution::details::Callback * callback_p) noexcept
        {
            auto & operation = *static_cast<Self *>(callback_p);
            operation.fail(operation.m_failure);
        }
        void fail(std::error_code error) noexcept
        {
            m_state_handle->publish(std::unexpected(error));
            this->complete();
        }
        void complete() noexcept { stde::set_value(std::move(m_receiver)); }
    private:
        StateHandle m_state_handle;
        Executor m_executor;
        Receiver m_receiver;
        Function m_function;
        std::error_code m_failure;
        std::optional<EntryHolder> m_entry_opt;
        std::optional<Holder> m_holder_opt;
    };
public:
    using sender_concept = stde::sender_t;
    using completion_signatures = stde::completion_signatures<stde::set_value_t()>;
public:
    ExecutionSender(StateHandle state_handle, Executor executor) noexcept : m_state_handle(std::move(state_handle)), m_executor(std::move(executor)) {}
public:
    Environment get_env() const noexcept { return {}; }
    template <stde::receiver Receiver>
    auto connect(Receiver receiver) && { return Operation<Receiver> {std::move(m_state_handle), std::move(m_executor), std::move(receiver)}; }
private:
    StateHandle m_state_handle;
    Executor m_executor;
};

template <result_value_c T>
class ResultToken
{
    template <result_value_c> friend class lcf::async::task::Chain;
    template <result_value_c...> friend class lcf::async::task::ChainJunction;
    using Self = ResultToken;
    using ExecutionStateHandle = chain::ExecutionStateHandle<T>;
public:
    using value_type = T;
public:
    ~ResultToken() noexcept = default;
    ResultToken() = delete;
    ResultToken(const Self &) noexcept = default;
    ResultToken(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
private:
    explicit ResultToken(ExecutionStateHandle execution_state_handle) noexcept : m_execution_state_handle(std::move(execution_state_handle)) {}
    bool isReady() const noexcept { return m_execution_state_handle->isReady(); }
    const Result<T> & result() const noexcept { return m_execution_state_handle->result(); }
private:
    ExecutionStateHandle m_execution_state_handle;
};

template <typename Token>
concept result_token_c = requires {
    typename std::remove_cvref_t<Token>::value_type;
    requires std::same_as<std::remove_cvref_t<Token>, ResultToken<typename std::remove_cvref_t<Token>::value_type>>;
};

}
