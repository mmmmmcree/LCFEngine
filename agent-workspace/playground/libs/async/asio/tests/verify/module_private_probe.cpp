#include <async/task.h>

// This translation unit must fail: the helper is reachable to the compiler,
// but it is not exported for lookup by a module consumer.
auto private_helper = &lcf::async::task::details::exception_error;
