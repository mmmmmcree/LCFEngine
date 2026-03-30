#pragma once

#include "tasks_enums.h"

namespace lcf {
    class TaskSchedulerCreateInfo
    {
        using Self = TaskSchedulerCreateInfo;
    public:
        TaskSchedulerCreateInfo(
            TaskSchedulerRunMode run_mode = TaskSchedulerRunMode::eThisThread
        ) : m_run_mode(run_mode)
        {}
        Self & setRunMode(TaskSchedulerRunMode run_mode) noexcept { m_run_mode = run_mode; return *this; }
        TaskSchedulerRunMode getRunMode() const noexcept { return m_run_mode; }
    private:
        TaskSchedulerRunMode m_run_mode;
    };
}