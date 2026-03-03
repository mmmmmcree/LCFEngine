#include "tasks/TaskScheduler.h"
#include "tasks/tasks_create_infos.h"
#include "log.h"

using namespace lcf;

TaskScheduler::TaskScheduler(const TaskSchedulerCreateInfo & info)
{
    switch (info.getRunMode()) {
        case TaskSchedulerRunMode::eThisThread: { m_io_context_up = std::make_unique<IOContext>(); } break;
        case TaskSchedulerRunMode::eNewThread: { m_io_context_up = std::make_unique<ThreadIOContext>(); } break;
    }
}

TaskScheduler & TaskScheduler::registerAwaitable(asio::awaitable<void> awaitable)
{
    asio::co_spawn(this->getIOContext(), std::move(awaitable), &TaskScheduler::rethrow_exception);
    return *this;
}

void TaskScheduler::rethrow_exception(std::exception_ptr execption_p)
{
    if (not execption_p) { return; }
    try {
        std::rethrow_exception(execption_p);
    } catch (const std::exception & e) {
        lcf_log_error(e.what());
        throw e;
    }
}
