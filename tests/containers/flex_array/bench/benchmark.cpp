// FlexArray_Opus_4_7 vs FlexArray vs std::vector benchmarks (google-benchmark).
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

#include "array/FlexArray.h"
#include "array/FlexArray_Opus_4_7.h"
#include <log.h>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace {

    constexpr int N_small = 100'000;
    constexpr int N_large = 1'000'000;
    constexpr int N_mid = 5'000;

    // ============================================================
    // Helper: container trait adapters for template benchmarks
    // ============================================================
    template <typename C>
    struct BenchOps;

    template <typename T, typename A>
    struct BenchOps<std::vector<T, A>>
    {
        static void push(std::vector<T, A> & c, const T & v) { c.push_back(v); }
        static void reserve(std::vector<T, A> & c, std::size_t n) { c.reserve(n); }
        static std::string name() { return "std::vector"; }
    };

    template <typename T, std::unsigned_integral S, typename A>
    struct BenchOps<lcf::FlexArray<T, S, A>>
    {
        static void push(lcf::FlexArray<T, S, A> & c, const T & v) { c.push_back(v); }
        static void reserve(lcf::FlexArray<T, S, A> & c, std::size_t n) { c.reserve(n); }
        static std::string name() { return "FlexArray"; }
    };

    template <typename T, std::unsigned_integral S, typename A>
    struct BenchOps<lcf::FlexArray_Opus_4_7<T, S, A>>
    {
        static void push(lcf::FlexArray_Opus_4_7<T, S, A> & c, const T & v) { c.push_back(v); }
        static void reserve(lcf::FlexArray_Opus_4_7<T, S, A> & c, std::size_t n) { c.reserve(n); }
        static std::string name() { return "Opus_4_7"; }
    };

    // ============================================================
    // B.1  push_back without reserve
    // ============================================================
    template <typename Container>
    static void BM_PushBackNoReserve(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            Container c;
            for (int i = 0; i < n; ++i) { BenchOps<Container>::push(c, i); }
            benchmark::DoNotOptimize(c);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * n);
    }

    BENCHMARK(BM_PushBackNoReserve<std::vector<int>>)->Arg(N_small)->Name("PushBack_NoReserve/std::vector");
    BENCHMARK(BM_PushBackNoReserve<lcf::FlexArray<int>>)->Arg(N_small)->Name("PushBack_NoReserve/FlexArray");
    BENCHMARK(BM_PushBackNoReserve<lcf::FlexArray_Opus_4_7<int>>)->Arg(N_small)->Name("PushBack_NoReserve/Opus_4_7");

    // ============================================================
    // B.2  push_back with reserve
    // ============================================================
    template <typename Container>
    static void BM_PushBackReserved(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            Container c;
            BenchOps<Container>::reserve(c, static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { BenchOps<Container>::push(c, i); }
            benchmark::DoNotOptimize(c);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * n);
    }

    BENCHMARK(BM_PushBackReserved<std::vector<int>>)->Arg(N_small)->Name("PushBack_Reserved/std::vector");
    BENCHMARK(BM_PushBackReserved<lcf::FlexArray<int>>)->Arg(N_small)->Name("PushBack_Reserved/FlexArray");
    BENCHMARK(BM_PushBackReserved<lcf::FlexArray_Opus_4_7<int>>)->Arg(N_small)->Name("PushBack_Reserved/Opus_4_7");

    // ============================================================
    // B.3  Copy construction
    // ============================================================
    template <typename Container>
    static void BM_CopyCtor(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        Container src;
        BenchOps<Container>::reserve(src, static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) { BenchOps<Container>::push(src, i); }
        for (auto _ : state) {
            Container copy = src;
            benchmark::DoNotOptimize(copy);
            benchmark::ClobberMemory();
        }
        state.SetBytesProcessed(state.iterations() * n * static_cast<int64_t>(sizeof(int)));
    }

    BENCHMARK(BM_CopyCtor<std::vector<int>>)->Arg(N_large)->Name("CopyCtor/std::vector");
    BENCHMARK(BM_CopyCtor<lcf::FlexArray<int>>)->Arg(N_large)->Name("CopyCtor/FlexArray");
    BENCHMARK(BM_CopyCtor<lcf::FlexArray_Opus_4_7<int>>)->Arg(N_large)->Name("CopyCtor/Opus_4_7");

    // ============================================================
    // B.4  Move construction
    // ============================================================
    template <typename Container>
    static void BM_MoveCtor(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            state.PauseTiming();
            Container src;
            BenchOps<Container>::reserve(src, static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { BenchOps<Container>::push(src, i); }
            state.ResumeTiming();
            Container moved = std::move(src);
            benchmark::DoNotOptimize(moved);
            benchmark::ClobberMemory();
        }
    }

    BENCHMARK(BM_MoveCtor<std::vector<int>>)->Arg(N_large)->Name("MoveCtor/std::vector");
    BENCHMARK(BM_MoveCtor<lcf::FlexArray<int>>)->Arg(N_large)->Name("MoveCtor/FlexArray");
    BENCHMARK(BM_MoveCtor<lcf::FlexArray_Opus_4_7<int>>)->Arg(N_large)->Name("MoveCtor/Opus_4_7");

    // ============================================================
    // B.5  Iteration / sum
    // ============================================================
    template <typename Container>
    static void BM_IterSum(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        Container src;
        BenchOps<Container>::reserve(src, static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) { BenchOps<Container>::push(src, i); }
        for (auto _ : state) {
            long long s = 0;
            for (int x : src) { s += x; }
            benchmark::DoNotOptimize(s);
        }
        state.SetItemsProcessed(state.iterations() * n);
    }

    BENCHMARK(BM_IterSum<std::vector<int>>)->Arg(N_large)->Name("IterSum/std::vector");
    BENCHMARK(BM_IterSum<lcf::FlexArray<int>>)->Arg(N_large)->Name("IterSum/FlexArray");
    BENCHMARK(BM_IterSum<lcf::FlexArray_Opus_4_7<int>>)->Arg(N_large)->Name("IterSum/Opus_4_7");

    // ============================================================
    // B.6  Insert at middle (100 ops on N=5k)
    // ============================================================
    template <typename Container>
    static void BM_InsertMiddle(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        constexpr int ops = 100;
        for (auto _ : state) {
            state.PauseTiming();
            Container c;
            BenchOps<Container>::reserve(c, static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { BenchOps<Container>::push(c, i); }
            state.ResumeTiming();
            for (int i = 0; i < ops; ++i) { c.insert(c.cbegin() + c.size() / 2, i); }
            benchmark::DoNotOptimize(c);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * ops);
    }

    BENCHMARK(BM_InsertMiddle<std::vector<int>>)->Arg(N_mid)->Name("InsertMiddle/std::vector");
    BENCHMARK(BM_InsertMiddle<lcf::FlexArray<int>>)->Arg(N_mid)->Name("InsertMiddle/FlexArray");
    BENCHMARK(BM_InsertMiddle<lcf::FlexArray_Opus_4_7<int>>)->Arg(N_mid)->Name("InsertMiddle/Opus_4_7");

    // ============================================================
    // B.7  Erase at middle (100 ops on N=5k)
    // ============================================================
    template <typename Container>
    static void BM_EraseMiddle(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        constexpr int ops = 100;
        for (auto _ : state) {
            state.PauseTiming();
            Container c;
            BenchOps<Container>::reserve(c, static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) { BenchOps<Container>::push(c, i); }
            state.ResumeTiming();
            for (int i = 0; i < ops; ++i) { c.erase(c.cbegin() + c.size() / 2); }
            benchmark::DoNotOptimize(c);
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * ops);
    }

    BENCHMARK(BM_EraseMiddle<std::vector<int>>)->Arg(N_mid)->Name("EraseMiddle/std::vector");
    BENCHMARK(BM_EraseMiddle<lcf::FlexArray<int>>)->Arg(N_mid)->Name("EraseMiddle/FlexArray");
    BENCHMARK(BM_EraseMiddle<lcf::FlexArray_Opus_4_7<int>>)->Arg(N_mid)->Name("EraseMiddle/Opus_4_7");

    // ============================================================
    // B.8  resize(N, 42) from empty
    // ============================================================
    template <typename Container>
    static void BM_Resize(benchmark::State & state)
    {
        const int n = static_cast<int>(state.range(0));
        for (auto _ : state) {
            Container c;
            c.resize(static_cast<std::size_t>(n), 42);
            benchmark::DoNotOptimize(c);
            benchmark::ClobberMemory();
        }
        state.SetBytesProcessed(state.iterations() * n * static_cast<int64_t>(sizeof(int)));
    }

    BENCHMARK(BM_Resize<std::vector<int>>)->Arg(N_small)->Name("Resize/std::vector");
    BENCHMARK(BM_Resize<lcf::FlexArray<int>>)->Arg(N_small)->Name("Resize/FlexArray");
    BENCHMARK(BM_Resize<lcf::FlexArray_Opus_4_7<int>>)->Arg(N_small)->Name("Resize/Opus_4_7");

} // namespace

int main(int argc, char ** argv)
{
    lcf::Logger::init();
    lcf_log_info("Three-way benchmark: std::vector vs FlexArray vs FlexArray_Opus_4_7");
    lcf_log_info("  sizeof(std::vector<int>)           = {} bytes", sizeof(std::vector<int>));
    lcf_log_info("  sizeof(lcf::FlexArray<int>)        = {} bytes", sizeof(lcf::FlexArray<int>));
    lcf_log_info("  sizeof(lcf::FlexArray_Opus_4_7<int>) = {} bytes", sizeof(lcf::FlexArray_Opus_4_7<int>));

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) { return 1; }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    lcf_log_info("Benchmark suite finished");
    return 0;
}
