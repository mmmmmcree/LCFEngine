#include "Workloads.h"
#include <benchmark/benchmark.h>

namespace {

namespace comparison = lcf::async::comparison;

void workload(benchmark::State & state, comparison::Case test_case)
{
    try {
        for (auto iteration : state) { comparison::run_case(test_case); }
        state.SetItemsProcessed(state.iterations() * test_case.m_size);
    } catch (const std::exception & error) {
        state.SkipWithError(error.what());
    }
}

void dispatch_ready(benchmark::State & state)
{
    lcf::async::testing::Context context;
    unsigned calls = 0;
    for (auto iteration : state) {
        state.PauseTiming();
        context.restart();
        calls = 0;
        for (unsigned index = 0; index < 256; ++index) {
            lcf::async::testing::post(context, [&] { ++calls; });
        }
        state.ResumeTiming();
        while (context.poll_one()) {}
        benchmark::DoNotOptimize(calls);
        if (calls != 256) { state.SkipWithError("incorrect dispatch count"); break; }
    }
    state.SetItemsProcessed(state.iterations() * 256);
}

BENCHMARK(dispatch_ready)->UseRealTime()->MinTime(0.05);

const bool k_registered = [] {
    for (const auto test_case : comparison::k_cases) {
        benchmark::RegisterBenchmark(test_case.m_name.data(), workload, test_case)->UseRealTime()->MinTime(0.05);
    }
    return true;
}();

}

BENCHMARK_MAIN();
