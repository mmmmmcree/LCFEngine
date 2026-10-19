#include <async/execution.h>
#include <async/task.h>

#if defined(LCF_ASYNC_PROBE_FRAME)
auto private_helper = &lcf::async::execution::details::allocate_frame;
#elif defined(LCF_ASYNC_PROBE_QUEUE)
using PrivateQueue = lcf::async::execution::details::Queue;
#else
auto private_helper = &lcf::async::task::details::exception_error;
#endif
