#pragma once

#include "chain_details.h"
#include <async/core/cancellation.h>

namespace lcf::async::task::details {

template <typename T, typename Completion>
class Delivery final : public ::lcf::async::execution::details::Callback
{
    using Self = Delivery;
    using StateHandle = chain::ExecutionStateHandle<T>;
    struct Environment
    {
        Executor m_executor;
        stde::inplace_stop_token m_stop_token;
        Executor query(stde::get_start_scheduler_t) const noexcept { return m_executor; }
        stde::inplace_stop_token query(stde::get_stop_token_t) const noexcept { return m_stop_token; }
    };
    struct Receiver
    {
        using receiver_concept = stde::receiver_t;
        Environment get_env() const noexcept { return {m_delivery_p->m_executor, m_delivery_p->m_stop_source.get_token()}; }
        void set_value(ResultHandle<T> result_rh) noexcept
        {
            m_delivery_p->m_result_rh = std::move(result_rh);
            m_delivery_p->m_executor.enqueue(m_delivery_p);
        }
        void set_error(std::exception_ptr exception) noexcept
        {
            auto & state = *m_delivery_p->m_state_handle;
            if (not state.isReady()) {
                state.m_operation = {};
                state.publish(std::unexpected(exception_error(std::move(exception))));
            }
            this->set_value(make_result_handle(m_delivery_p->m_state_handle, &state.result()));
        }
        void set_stopped() noexcept
        {
            auto & state = *m_delivery_p->m_state_handle;
            if (not state.isReady()) {
                state.m_operation = {};
                state.publish(std::unexpected(std::make_error_code(std::errc::operation_canceled)));
            }
            this->set_value(make_result_handle(m_delivery_p->m_state_handle, &state.result()));
        }
        Self * m_delivery_p;
    };
    using Operation = stde::connect_result_t<Awaitable<ResultHandle<T>>, Receiver>;
    struct Holder
    {
        Holder(Awaitable<ResultHandle<T>> task, Self * delivery_p) :
            m_operation(stde::connect(std::move(task), Receiver {delivery_p})) {}
        Operation m_operation;
    };
    struct Cancel
    {
        Self * m_delivery_p;
        void operator()() const noexcept { m_delivery_p->m_stop_source.request_stop(); }
    };
public:
    ~Delivery() noexcept { m_executor.workFinished(); }
    Delivery(Executor executor, StateHandle state_handle, Completion completion) :
        ::lcf::async::execution::details::Callback {&Self::invoke}, m_executor(std::move(executor)),
        m_state_handle(std::move(state_handle)), m_completion(std::move(completion))
    {
        if constexpr (requires { { m_completion.getStopToken() } -> std::same_as<std::stop_token>; }) {
            m_cancellation.emplace(m_completion.getStopToken(), Cancel {this});
        }
        m_executor.workStarted();
    }
    void prepare(Awaitable<ResultHandle<T>> task) { m_holder_opt.emplace(std::move(task), this); }
    void start() noexcept
    {
        stde::start(m_holder_opt->m_operation);
    }
private:
    static void invoke(::lcf::async::execution::details::Callback * callback_p)
    {
        std::unique_ptr<Self> delivery_up {static_cast<Self *>(callback_p)};
        delivery_up->m_holder_opt.reset();
        delivery_up->m_cancellation.reset();
        std::invoke(delivery_up->m_completion, *delivery_up->m_result_rh);
    }
private:
    Executor m_executor;
    StateHandle m_state_handle;
    Completion m_completion;
    ResultHandle<T> m_result_rh;
    stde::inplace_stop_source m_stop_source;
    std::optional<std::stop_callback<Cancel>> m_cancellation;
    std::optional<Holder> m_holder_opt;
};

template <typename T, typename Completion>
void start_owned(Executor executor, Awaitable<ResultHandle<T>> task,
    chain::ExecutionStateHandle<T> state_handle, Completion completion)
{
    auto delivery_up = std::make_unique<Delivery<T, Completion>>(
        std::move(executor), std::move(state_handle), std::move(completion));
    delivery_up->prepare(std::move(task));
    auto * delivery_p = delivery_up.release();
    delivery_p->start();
}

}
