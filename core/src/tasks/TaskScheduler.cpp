#include "tasks/TaskScheduler.h"
#include "log.h"

void lcf::TaskScheduler::rethrow_exception(std::exception_ptr execption_p)
{
    if (not execption_p) { return; }
    try {
        std::rethrow_exception(execption_p);
    } catch (const std::exception & e) {
        lcf_log_error(e.what());
        throw e;
    }
}
