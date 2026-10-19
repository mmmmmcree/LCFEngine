#include <async/core/Context.h>
#include <async/core/Awaitable.h>
#include <async/core/Executor.h>
#include <async/task.h>
#include <iostream>

int main()
{
    namespace task = lcf::async::task;
#if defined(LCF_EXAMPLE_STDEXEC)
    lcf::async::Context context;
    const auto executor = context.getExecutor();
#else
    lcf::async::Context context;
    const auto executor = context.get_executor();
#endif
    int exit_code = 1;
    auto chain = task::Chain {20, executor}
        .link([](const int & value) -> lcf::async::Awaitable<int> { co_return value + 1; })
        .link([](const int & value) { return value * 2; });
    std::move(chain).start(executor, [&](const task::Result<int> & result) {
        if (not result) {
            std::cerr << result.error().message() << '\n';
            return;
        }
        std::cout << "async result = " << *result << '\n';
        exit_code = *result == 42 ? 0 : 1;
    });
#if defined(LCF_EXAMPLE_STDEXEC)
    context.runUntilIdle();
#else
    context.run();
#endif
    return exit_code;
}
