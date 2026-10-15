module;

#define LCF_ASYNC_FORCE_HEADERS
#include <async/task.h>
#undef LCF_ASYNC_FORCE_HEADERS

export module lcf.async;

export namespace lcf::async::task {

using ::lcf::async::task::Chain;
using ::lcf::async::task::Flow;
using ::lcf::async::task::Result;
using ::lcf::async::task::ResultView;
using ::lcf::async::task::State;
using ::lcf::async::task::as_flow;
using ::lcf::async::task::merge;

}
