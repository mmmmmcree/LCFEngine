module;

#define LCF_ASYNC_FORCE_HEADERS
#include <async/task.h>
#undef LCF_ASYNC_FORCE_HEADERS

export module lcf.async;

export namespace lcf::async {

using ::lcf::async::Awaitable;
using ::lcf::async::Executor;
using ::lcf::async::Context;
using ::lcf::async::Signal;
using ::lcf::async::CancellationSource;
using ::lcf::async::post;
using ::lcf::async::yield;
using ::lcf::async::delay;
using ::lcf::async::bind_cancellation;
using ::lcf::async::request_cancellation;

}

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
