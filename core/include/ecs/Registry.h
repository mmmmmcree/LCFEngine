#pragma once

#include "ecs_fwd_decls.h"
#include "Dispatcher.h"
#include "tasks/TaskScheduler.h"
#include "tasks/tasks_create_infos.h"
#include <optional>

namespace lcf::ecs {
    class RegistryCreateInfo;

    class Registry : public BasicRegistry
    {
        using Base = BasicRegistry;
    public:
        Registry(const RegistryCreateInfo & info);
        using Base::Base;
    public:
        template <typename Signal>
        void emitSignal(Signal && signal)
        {
            auto & task_scheduler = this->ctx().get<TaskScheduler>();
            auto & dispatcher = this->ctx().get<Dispatcher>();
            task_scheduler.post([&dispatcher, signal = std::forward<Signal>(signal)]() {
                dispatcher.trigger(signal);
            });
        }
        template <typename Signal>
        void enqueueSignal(Signal && signal)
        {
            auto & dispatcher = this->ctx().get<Dispatcher>();
            dispatcher.enqueue(std::forward<Signal>(signal));
        }
    };


    class RegistryCreateInfo
    {
        using Self = RegistryCreateInfo;
    public:
        RegistryCreateInfo(
            std::optional<TaskSchedulerCreateInfo> task_scheduler_info_opt = std::nullopt
        ) : m_task_scheduler_info_opt(task_scheduler_info_opt)
        {}
        Self & setTaskSchedulerInfo(const TaskSchedulerCreateInfo & task_scheduler_info) noexcept { m_task_scheduler_info_opt = task_scheduler_info; return * this; }
        const std::optional<TaskSchedulerCreateInfo> & getTaskSchedulerInfoOpt() const noexcept { return m_task_scheduler_info_opt; }
    private:
        std::optional<TaskSchedulerCreateInfo> m_task_scheduler_info_opt;
    };
}