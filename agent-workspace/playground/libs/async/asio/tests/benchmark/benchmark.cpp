#include <mutex>
#include <asio.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <expected>
#include <asio/io_context.hpp>
#include <asio/version.hpp>
#include <benchmark/benchmark.h>
#include <iostream>
#include <string>

#include <async/task.h>

namespace {

namespace task = lcf::async::task;

task::Chain<int> make_chain(asio::any_io_executor executor, int length)
{
    auto chain = task::Chain {0, executor};
    for (int index = 0; index < length; ++index) {
        chain = std::move(chain).link([](const int & value) { return value + 1; });
    }
    return chain;
}

asio::awaitable<int> post_loop(int length)
{
    int value = 0;
    for (int index = 0; index < length; ++index) {
        co_await asio::post(asio::use_awaitable);
        ++value;
    }
    co_return value;
}

void chain_build_and_destroy(benchmark::State & state)
{
    asio::io_context context;
    const auto length = static_cast<int>(state.range(0));
    for (auto iteration : state) {
        auto chain = make_chain(context.get_executor(), length);
        benchmark::DoNotOptimize(chain);
    }
    state.SetItemsProcessed(state.iterations() * length);
}

void chain_start_and_run(benchmark::State & state)
{
    asio::io_context context;
    const auto length = static_cast<int>(state.range(0));
    for (auto iteration : state) {
        state.PauseTiming();
        context.restart();
        auto chain = make_chain(context.get_executor(), length);
        int output = -1;
        state.ResumeTiming();
        std::move(chain).start(context.get_executor(), [&](const task::Result<int> & result) {
            if (result) { output = result.value(); }
        });
        context.run();
        benchmark::DoNotOptimize(output);
        if (output != length) {
            state.SkipWithError("chain produced an incorrect result");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations() * length);
}

void asio_post_loop(benchmark::State & state)
{
    asio::io_context context;
    const auto length = static_cast<int>(state.range(0));
    for (auto iteration : state) {
        state.PauseTiming();
        context.restart();
        int output = -1;
        state.ResumeTiming();
        asio::co_spawn(context, post_loop(length), [&](std::exception_ptr exception, int result) {
            if (not exception) { output = result; }
        });
        context.run();
        benchmark::DoNotOptimize(output);
        if (output != length) {
            state.SkipWithError("Asio baseline produced an incorrect result");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations() * length);
}

void merge_two_chains(benchmark::State & state)
{
    asio::io_context context;
    const auto length = static_cast<int>(state.range(0));
    for (auto iteration : state) {
        state.PauseTiming();
        context.restart();
        auto first = make_chain(context.get_executor(), length);
        auto second = make_chain(context.get_executor(), length);
        auto joined = task::merge(first.getResultToken(), second.getResultToken())
            .link(context.get_executor(), [](const int & a, const int & b) { return a + b; });
        int output = -1;
        state.ResumeTiming();
        std::move(first).start(context.get_executor(), [](const auto &) {});
        std::move(second).start(context.get_executor(), [](const auto &) {});
        std::move(joined).start(context.get_executor(), [&](const task::Result<int> & result) {
            if (result) { output = result.value(); }
        });
        context.run();
        benchmark::DoNotOptimize(output);
        if (output != 2 * length) {
            state.SkipWithError("merge produced an incorrect result");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations() * 2 * length);
}

BENCHMARK(chain_build_and_destroy)->Arg(1)->Arg(8)->Arg(64)->UseRealTime();
BENCHMARK(chain_start_and_run)->Arg(1)->Arg(8)->Arg(64)->UseRealTime();
BENCHMARK(asio_post_loop)->Arg(1)->Arg(8)->Arg(64)->UseRealTime();
BENCHMARK(merge_two_chains)->Arg(1)->Arg(8)->Arg(64)->UseRealTime();

}

int main(int argc, char ** argv)
{
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) { return 1; }
    benchmark::AddCustomContext("build_configuration", LCF_ASYNC_BENCHMARK_CONFIGURATION);
    benchmark::AddCustomContext("implementation_sha256", LCF_ASYNC_IMPLEMENTATION_HASH);
    benchmark::AddCustomContext("implementation", LCF_ASYNC_IMPLEMENTATION);
    benchmark::AddCustomContext("asio_version", std::to_string(ASIO_VERSION));
    benchmark::AddCustomContext("executor_threads", "1");
    benchmark::AddCustomContext("timing", "real time; graph construction excluded from start/run cases");
    if (std::string {LCF_ASYNC_BENCHMARK_CONFIGURATION} != "Release") {
        std::cerr << "Use the Release build preset when comparing performance.\n";
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
}
