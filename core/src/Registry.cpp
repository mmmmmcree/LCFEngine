#include "Registry.h"
#include "tasks/TaskScheduler.h"

using namespace lcf;

Registry::Registry(const RegistryCreateInfo & info) : Base()
{
    if (info.getTaskSchedulerInfoOpt()) {
        const TaskSchedulerCreateInfo & task_scheduler_info = *info.getTaskSchedulerInfoOpt();
        this->ctx().emplace<TaskScheduler>(task_scheduler_info); //! must emplace before dispatcher? don't know why
    }
    this->ctx().emplace<Dispatcher>();
}