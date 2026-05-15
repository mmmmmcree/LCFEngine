// FlexArray vs std::vector benchmarks driven by google-benchmark.
//
// Standalone executable (NOT registered with CTest). Run manually:
//   .\containers_flex_array_bench.exe                 (Windows)
//   ./containers_flex_array_bench                      (Linux)
//
// Useful flags (provided by google-benchmark):
//   --benchmark_filter=PushBack.*       Run a subset by name regex.
//   --benchmark_min_time=0.5s           Minimum measurement time per case.
//   --benchmark_repetitions=5           Repeat for stddev/median.
//   --benchmark_format=json             Machine-readable output.
//   --benchmark_out=results.json        Write results to file.
//
// google-benchmark already provides DoNotOptimize/ClobberMemory and an
// adaptive iteration policy, so we no longer need the hand-rolled
// best-of-5 / [[gnu::noinline]] kernel scaffolding from the previous
// implementation.

#include <array/FlexArray.h>
#include <log.h>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

namespace {

    constexpr int N_small = 100'000;
    constexpr int N_large = 1'000'000;
    constexpr int N_mid = 5'000;

    // ============================================================
    // B.1  pushBack without reserve
    // ============================================================
    static void BM_VectorPushBackNoReserve(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            std::vector<int> v;
            for (int i = 0; i < n; ++i) { v.push_back(i); }
            benchmark::DoNotOptimize(v);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * n);
    }
    static void BM_FlexArrayPushBackNoReserve(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            lcf::FlexArray<int> a;
            for (int i = 0; i < n; ++i) { a.pushBack(i); }
            benchmark::DoNotOptimize(a);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * n);
    }
    BENCHMARK(BM_VectorPushBackNoReserve)->Arg(N_small);
    BENCHMARK(BM_FlexArrayPushBackNoReserve)->Arg(N_small);

    // ============================================================
    // B.2  pushBack with reserve
    // ============================================================
    static void BM_VectorPushBackReserved(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            std::vector<int> v;
            v.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { v.push_back(i); }
            benchmark::DoNotOptimize(v);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * n);
    }
    static void BM_FlexArrayPushBackReserved(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            lcf::FlexArray<int> a;
            a.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { a.pushBack(i); }
            benchmark::DoNotOptimize(a);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * n);
    }
    BENCHMARK(BM_VectorPushBackReserved)->Arg(N_small);
    BENCHMARK(BM_FlexArrayPushBackReserved)->Arg(N_small);

    // ============================================================
    // B.3  Copy construction
    // ============================================================
    static void BM_VectorCopyCtor(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        std::vector<int> src(n);
        std::iota(src.begin(), src.end(), 0);
        for (auto _ : state) {
            std::vector<int> copy = src;
            benchmark::DoNotOptimize(copy);
            benchmark::ClobberMemory();
        }
        state.SetBytesProcessed(state.iterations() * n * sizeof(int));
    }
    static void BM_FlexArrayCopyCtor(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        lcf::FlexArray<int> src;
        src.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) { src.pushBack(i); }
        for (auto _ : state) {
            lcf::FlexArray<int> copy = src;
            benchmark::DoNotOptimize(copy);
            benchmark::ClobberMemory();
        }
        state.SetBytesProcessed(state.iterations() * n * sizeof(int));
    }
    BENCHMARK(BM_VectorCopyCtor)->Arg(N_large);
    BENCHMARK(BM_FlexArrayCopyCtor)->Arg(N_large);

    // ============================================================
    // B.4  Move construction
    // ============================================================
    static void BM_VectorMoveCtor(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            state.PauseTiming();
            std::vector<int> src(static_cast<std::size_t>(n), 7);
            state.ResumeTiming();
            std::vector<int> moved = std::move(src);
            benchmark::DoNotOptimize(moved);
            benchmark::ClobberMemory();
        }
    }
    static void BM_FlexArrayMoveCtor(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            state.PauseTiming();
            lcf::FlexArray<int> src(static_cast<std::size_t>(n), 7);
            state.ResumeTiming();
            lcf::FlexArray<int> moved = std::move(src);
            benchmark::DoNotOptimize(moved);
            benchmark::ClobberMemory();
        }
    }
    BENCHMARK(BM_VectorMoveCtor)->Arg(N_large);
    BENCHMARK(BM_FlexArrayMoveCtor)->Arg(N_large);

    // ============================================================
    // B.5  Iteration / sum
    // ============================================================
    static void BM_VectorIterSum(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        std::vector<int> src(n);
        std::iota(src.begin(), src.end(), 0);
        for (auto _ : state) {
            long long s = 0;
            for (int x : src) { s += x; }
            benchmark::DoNotOptimize(s);
        }
        state.SetItemsProcessed(state.iterations() * n);
    }
    static void BM_FlexArrayIterSum(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        lcf::FlexArray<int> src;
        src.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) { src.pushBack(i); }
        for (auto _ : state) {
            long long s = 0;
            for (int x : src) { s += x; }
            benchmark::DoNotOptimize(s);
        }
        state.SetItemsProcessed(state.iterations() * n);
    }
    BENCHMARK(BM_VectorIterSum)->Arg(N_large);
    BENCHMARK(BM_FlexArrayIterSum)->Arg(N_large);

    // ============================================================
    // B.6  Insert at middle (100 ops on N=5k)
    // ============================================================
    static void BM_VectorInsertMiddle(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        constexpr int ops = 100;
        for (auto _ : state) {
            state.PauseTiming();
            std::vector<int> v;
            v.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { v.push_back(i); }
            state.ResumeTiming();
            for (int i = 0; i < ops; ++i) { v.insert(v.begin() + v.size() / 2, i); }
            benchmark::DoNotOptimize(v);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * ops);
    }
    static void BM_FlexArrayInsertMiddle(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        constexpr int ops = 100;
        for (auto _ : state) {
            state.PauseTiming();
            lcf::FlexArray<int> a;
            a.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { a.pushBack(i); }
            state.ResumeTiming();
            for (int i = 0; i < ops; ++i) { a.insert(a.cbegin() + a.size() / 2, i); }
            benchmark::DoNotOptimize(a);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * ops);
    }
    BENCHMARK(BM_VectorInsertMiddle)->Arg(N_mid);
    BENCHMARK(BM_FlexArrayInsertMiddle)->Arg(N_mid);

    // ============================================================
    // B.7  Erase at middle (100 ops on N=5k)
    // ============================================================
    static void BM_VectorEraseMiddle(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        constexpr int ops = 100;
        for (auto _ : state) {
            state.PauseTiming();
            std::vector<int> v;
            v.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { v.push_back(i); }
            state.ResumeTiming();
            for (int i = 0; i < ops; ++i) { v.erase(v.begin() + v.size() / 2); }
            benchmark::DoNotOptimize(v);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * ops);
    }
    static void BM_FlexArrayEraseMiddle(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        constexpr int ops = 100;
        for (auto _ : state) {
            state.PauseTiming();
            lcf::FlexArray<int> a;
            a.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { a.pushBack(i); }
            state.ResumeTiming();
            for (int i = 0; i < ops; ++i) { a.erase(a.cbegin() + a.size() / 2); }
            benchmark::DoNotOptimize(a);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * ops);
    }
    BENCHMARK(BM_VectorEraseMiddle)->Arg(N_mid);
    BENCHMARK(BM_FlexArrayEraseMiddle)->Arg(N_mid);

    // ============================================================
    // B.8  resize(N, 42) from empty
    // ============================================================
    static void BM_VectorResize(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            std::vector<int> v;
            v.resize(static_cast<std::size_t>(n), 42);
            benchmark::DoNotOptimize(v);
            benchmark::ClobberMemory();
        }
        state.SetBytesProcessed(state.iterations() * n * sizeof(int));
    }
    static void BM_FlexArrayResize(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            lcf::FlexArray<int> a;
            a.resize(static_cast<std::size_t>(n), 42);
            benchmark::DoNotOptimize(a);
            benchmark::ClobberMemory();
        }
        state.SetBytesProcessed(state.iterations() * n * sizeof(int));
    }
    BENCHMARK(BM_VectorResize)->Arg(N_small);
    BENCHMARK(BM_FlexArrayResize)->Arg(N_small);

} // namespace

int main(int argc, char ** argv)
{
    lcf::Logger::init();
    lcf_log_info("FlexArray vs std::vector benchmark suite starting");
    lcf_log_info("  sizeof(std::vector<int>)    = {} bytes", sizeof(std::vector<int>));
    lcf_log_info("  sizeof(lcf::FlexArray<int>) = {} bytes", sizeof(lcf::FlexArray<int>));
    lcf_log_info("  FlexArray heap header       = {} bytes (size+capacity)",
                 2 * sizeof(std::uint32_t));

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) { return 1; }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    lcf_log_info("Benchmark suite finished");
    return 0;
}
