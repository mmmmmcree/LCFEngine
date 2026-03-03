#pragma once

#include "ecs_fwd_decls.h"
#include "tasks/tasks_create_infos.h"
#include <optional>

namespace lcf {
    class RegistryCreateInfo;

    class Registry : public entt::registry
    {
        using Base = entt::registry;
    public:
        Registry(const RegistryCreateInfo & info);
        using Base::Base;
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