module;

#define LCF_ASYNC_FORCE_HEADERS
#include <async/task.h>
#undef LCF_ASYNC_FORCE_HEADERS

export module lcf.async;

export namespace lcf::async::task {

using ::lcf::async::task::Chain;
using ::lcf::async::task::ChainJunction;
using ::lcf::async::task::ErrorAction;
using ::lcf::async::task::ErrorHandleContext;
using ::lcf::async::task::ErrorHandler;
using ::lcf::async::task::Result;
using ::lcf::async::task::ResultHandle;
using ::lcf::async::task::RetryAfter;
using ::lcf::async::task::make_result_handle;
using ::lcf::async::task::merge;
using ::lcf::async::task::on_error;
using ::lcf::async::task::propagate;
using ::lcf::async::task::retry_after;
using ::lcf::async::task::use_value;

}
