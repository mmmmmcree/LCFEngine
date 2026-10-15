#include <mutex>
#include <asio.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <expected>
#include <asio/io_context.hpp>
#include <memory>

#include <async/task.h>

namespace {

namespace task = lcf::async::task;

using Chain = task::Chain<int>;
using Flow = task::Flow<int>;

[[maybe_unused]] void instantiate_public_operations(asio::io_context & context)
{
    auto executor = context.get_executor();
    auto source = task::Chain {std::make_unique<int>(7), executor};
    auto original = source.getResult();
    auto left = std::move(source)
        .link([](const std::unique_ptr<int> & value) { return *value; })
        .link([](const int & value, const std::unique_ptr<int> & input) { return value + *input; }, original);
    auto shared = task::as_flow(std::move(left));
    auto branch = shared.link([](const int & value) { return value + 1; });
    auto joined = task::merge(std::move(branch), shared)
        .link([](const int & a, const int & b) { return a + b; });
    std::move(joined).start(executor, [](const task::Result<int> &) {});
}

}
