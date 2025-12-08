#include "tasks/TaskScheduler.h"
#include "log.h"

void lcf::TaskScheduler::handle_exception(std::exception_ptr exception_p, std::optional<ExceptionHandler> error_handler_opt)
{
    if (not exception_p) { return; }
    if (error_handler_opt) {
        (*error_handler_opt)(exception_p);
    } else {
        try { std::rethrow_exception(exception_p); }
        catch (const std::exception& ex) {
            lcf_log_error("Exception in async task: {}", ex.what());
        }
    }
}