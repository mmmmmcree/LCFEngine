#define LCF_ASYNC_FORCE_HEADERS
#include <async/task.h>
#undef LCF_ASYNC_FORCE_HEADERS

namespace lcf::async::testing {

task::Chain<int> make_header_chain(asio::any_io_executor executor)
{
    return task::Chain {40, std::move(executor)}
        .link([](const int & value) { return value + 1; });
}

task::Flow<int> share_header_chain(task::Chain<int> chain)
{
    return task::as_flow(std::move(chain));
}

}
