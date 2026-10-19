#include <async/task.h>
#include <benchmark/benchmark.h>

namespace {

void chain_round_trip(benchmark::State & state)
{
    namespace task = lcf::async::task;
    lcf::async::Context context;
    const auto executor = context.getExecutor();
    const auto depth = state.range(0);
    for (auto iteration : state) {
        auto chain = task::Chain {0, executor};
        for (std::int64_t index = 0; index < depth; ++index) {
            chain = std::move(chain).link([](const int & value) { return value + 1; });
        }
        std::move(chain).start(executor, [&](const task::Result<int> & result) {
            if (not result or result.value() != depth) { state.SkipWithError("incorrect chain result"); }
            benchmark::DoNotOptimize(result);
        });
        context.runUntilIdle();
    }
    state.SetItemsProcessed(state.iterations() * depth);
}

BENCHMARK(chain_round_trip)->Arg(1)->Arg(8)->Arg(32)->UseRealTime();

}

BENCHMARK_MAIN();
