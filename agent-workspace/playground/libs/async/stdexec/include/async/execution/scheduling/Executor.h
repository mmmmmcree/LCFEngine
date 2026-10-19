#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include <async/config.h>
#include <async/execution/stdexec.h>
#include "QueueScheduler.h"

namespace lcf::async::execution {

namespace details {

using SchedulerReceiver = exec::any_receiver<stde::completion_signatures<
    stde::set_value_t(), stde::set_error_t(std::exception_ptr), stde::set_stopped_t()>>;
using AnyScheduler = exec::any_scheduler<exec::any_sender<SchedulerReceiver>>;

template <typename Scheduler>
concept callback_scheduler_c = requires(const Scheduler & scheduler, Callback * callback_p) {
    { scheduler.enqueue(callback_p) } noexcept;
    { scheduler.workStarted() } noexcept;
    { scheduler.workFinished() } noexcept;
} and stde::scheduler<Scheduler>;

struct ExecutorState
{
    template <callback_scheduler_c Scheduler>
    explicit ExecutorState(Scheduler scheduler) :
        m_owner_sp(std::make_shared<Scheduler>(std::move(scheduler))),
        m_scheduler(*static_cast<const Scheduler *>(m_owner_sp.get())),
        m_enqueue([](const void * owner_p, Callback * callback_p) noexcept { static_cast<const Scheduler *>(owner_p)->enqueue(callback_p); }),
        m_work_started([](const void * owner_p) noexcept { static_cast<const Scheduler *>(owner_p)->workStarted(); }),
        m_work_finished([](const void * owner_p) noexcept { static_cast<const Scheduler *>(owner_p)->workFinished(); }),
        m_is_queue(std::same_as<Scheduler, QueueScheduler>) {}
    std::shared_ptr<const void> m_owner_sp;
    AnyScheduler m_scheduler;
    void (*m_enqueue)(const void *, Callback *) noexcept;
    void (*m_work_started)(const void *) noexcept;
    void (*m_work_finished)(const void *) noexcept;
    bool m_is_queue;
};

}

LCF_ASYNC_EXPORT
class Executor
{
    using Self = Executor;
    using StateHandle = std::shared_ptr<details::ExecutorState>;
    template <typename Receiver>
    class Operation final : public details::Callback
    {
        using Self = Operation;
        using ExternalOperation = stde::connect_result_t<stde::schedule_result_t<details::AnyScheduler>, Receiver>;
        union Storage
        {
            Storage() noexcept {}
            ~Storage() noexcept {}
            Receiver m_receiver;
            ExternalOperation m_external;
        };
    public:
        using operation_state_concept = stde::operation_state_t;
    public:
        ~Operation() noexcept
        {
            if (m_state_sp->m_is_queue) { std::destroy_at(&m_storage.m_receiver); }
            else { std::destroy_at(&m_storage.m_external); }
        }
        Operation(StateHandle state_sp, Receiver receiver) : details::Callback {&Self::invoke}, m_state_sp(std::move(state_sp))
        {
            if (m_state_sp->m_is_queue) { std::construct_at(&m_storage.m_receiver, std::move(receiver)); }
            else {
                ::new (static_cast<void *>(&m_storage.m_external)) ExternalOperation(
                    stde::connect(stde::schedule(m_state_sp->m_scheduler), std::move(receiver)));
            }
        }
        Operation(const Self &) = delete;
        Operation(Self &&) = delete;
        Self & operator=(const Self &) = delete;
        Self & operator=(Self &&) = delete;
    public:
        void start() noexcept
        {
            if (m_state_sp->m_is_queue) { m_state_sp->m_enqueue(m_state_sp->m_owner_sp.get(), this); }
            else { stde::start(m_storage.m_external); }
        }
    private:
        static void invoke(details::Callback * callback_p) noexcept
        {
            auto & operation = *static_cast<Self *>(callback_p);
            stde::set_value(std::move(operation.m_storage.m_receiver));
        }
    private:
        StateHandle m_state_sp;
        Storage m_storage;
    };
public:
    using scheduler_concept = stde::scheduler_t;
    struct ScheduleSender
    {
        using sender_concept = stde::sender_t;
        using completion_signatures = stde::completion_signatures<
            stde::set_value_t(), stde::set_error_t(std::exception_ptr), stde::set_stopped_t()>;
        struct Environment
        {
            StateHandle m_state_sp;
            Executor query(stde::get_completion_scheduler_t<stde::set_value_t>) const noexcept { return Executor {m_state_sp}; }
        };
        Environment get_env() const noexcept { return {m_state_sp}; }
        template <stde::receiver Receiver>
        auto connect(Receiver receiver) const { return Operation<Receiver> {m_state_sp, std::move(receiver)}; }
        StateHandle m_state_sp;
    };
public:
    Executor() noexcept = default;
    template <typename Scheduler>
    requires (not std::same_as<std::remove_cvref_t<Scheduler>, Self> and details::callback_scheduler_c<Scheduler>)
    Executor(Scheduler scheduler) : m_state_sp(std::make_shared<details::ExecutorState>(std::move(scheduler))) {}
    bool operator==(const Self & other) const noexcept
    {
        if (m_state_sp == other.m_state_sp) { return true; }
        if (not m_state_sp or not other.m_state_sp) { return false; }
        return m_state_sp->m_scheduler == other.m_state_sp->m_scheduler;
    }
public:
    ScheduleSender schedule() const noexcept
    {
        assert(m_state_sp and "a default-constructed Executor has no runtime; use Context::getExecutor() or adapt a callback scheduler");
        return {m_state_sp};
    }
    void enqueue(details::Callback * callback_p) const noexcept { m_state_sp->m_enqueue(m_state_sp->m_owner_sp.get(), callback_p); }
    void workStarted() const noexcept { m_state_sp->m_work_started(m_state_sp->m_owner_sp.get()); }
    void workFinished() const noexcept { m_state_sp->m_work_finished(m_state_sp->m_owner_sp.get()); }
    template <typename Function>
    void post(Function function) const
    {
        struct Record : details::Callback
        {
            explicit Record(Function function) : details::Callback {&Record::invoke}, m_function(std::move(function)) {}
            static void invoke(details::Callback * callback_p)
            {
                std::unique_ptr<Record> record_up {static_cast<Record *>(callback_p)};
                std::invoke(record_up->m_function);
            }
            Function m_function;
        };
        this->enqueue(new Record {std::move(function)});
    }
private:
    explicit Executor(StateHandle state_sp) noexcept : m_state_sp(std::move(state_sp)) {}
private:
    StateHandle m_state_sp;
};

static_assert(stde::scheduler<Executor>);

}
#endif
