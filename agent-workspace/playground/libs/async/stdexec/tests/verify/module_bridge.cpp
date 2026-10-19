#include <async/execution.h>
#include <async/task.h>

namespace lcf::async::testing {

task::Chain<int> make_module_chain(lcf::async::execution::Executor executor)
{
    return task::Chain {40, std::move(executor)}.link([](const int & input) { return input + 1; });
}

task::ChainJunction<int> describe_module_chain(const task::Chain<int> & chain)
{
    return task::merge(chain.getResultToken());
}

}
