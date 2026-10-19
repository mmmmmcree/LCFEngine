#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include <async/execution/stdexec.h>
#include "details/queue.h"

namespace lcf::async::execution {

LCF_ASYNC_EXPORT
class Context;

LCF_ASYNC_EXPORT
class QueueScheduler
{
    friend class Context;
    using Self = QueueScheduler;
    using QueueHandle = std::shared_ptr<details::Queue>;
    template <typename Receiver>
    struct Operation : details::Callback
    {
        using operation_state_concept = stde::operation_state_t;
        Operation(QueueHandle queue_sp, Receiver receiver) :
            details::Callback {&Operation::invoke}, m_queue_sp(std::move(queue_sp)), m_receiver(std::move(receiver)) {}
        void start() noexcept { m_queue_sp->push(this); }
        static void invoke(details::Callback * callback_p) noexcept
        {
            auto & operation = *static_cast<Operation *>(callback_p);
            stde::set_value(std::move(operation.m_receiver));
        }
        QueueHandle m_queue_sp;
        Receiver m_receiver;
    };
public:
    using scheduler_concept = stde::scheduler_t;
    struct ScheduleSender
    {
        using sender_concept = stde::sender_t;
        using completion_signatures = stde::completion_signatures<stde::set_value_t()>;
        struct Environment
        {
            QueueHandle m_queue_sp;
            QueueScheduler query(stde::get_completion_scheduler_t<stde::set_value_t>) const noexcept
            {
                return QueueScheduler {m_queue_sp};
            }
            constexpr stde::forward_progress_guarantee query(stde::get_forward_progress_guarantee_t) const noexcept
            {
                return stde::forward_progress_guarantee::parallel;
            }
        };
        Environment get_env() const noexcept { return {m_queue_sp}; }
        template <stde::receiver Receiver>
        auto connect(Receiver receiver) const { return Operation<Receiver> {m_queue_sp, std::move(receiver)}; }
        QueueHandle m_queue_sp;
    };
public:
    QueueScheduler() noexcept = default;
    bool operator==(const Self &) const noexcept = default;
    ScheduleSender schedule() const noexcept
    {
        assert(m_queue_sp and "a default-constructed QueueScheduler has no queue; use Context::getScheduler()");
        return {m_queue_sp};
    }
    void enqueue(details::Callback * callback_p) const noexcept { m_queue_sp->push(callback_p); }
    void workStarted() const noexcept { m_queue_sp->workStarted(); }
    void workFinished() const noexcept { m_queue_sp->workFinished(); }
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
    explicit QueueScheduler(QueueHandle queue_sp) noexcept : m_queue_sp(std::move(queue_sp)) {}
private:
    QueueHandle m_queue_sp;
};


}
#endif
