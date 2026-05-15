project(containers_flex_array_tests)

# ============================================================
# Unit tests (correctness) — registered with CTest
# Executable: containers_flex_array_unit_tests
#
# Run all FlexArray tests:
#   ctest -R "containers_flex_array"
#   .\containers_flex_array_unit_tests.exe
#
# Run a specific test (gtest filter):
#   .\containers_flex_array_unit_tests.exe --gtest_filter="FlexArrayException.*"
# ============================================================
add_executable(containers_flex_array_unit_tests
    unit/contract_test.cpp
    unit/specific_test.cpp
    unit/construction_test.cpp
    unit/assignment_test.cpp
    unit/modifiers_test.cpp
    unit/capacity_test.cpp
    unit/exception_test.cpp
    unit/specialization_test.cpp
    unit/const_test.cpp
)
target_compile_features(containers_flex_array_unit_tests PRIVATE cxx_std_23)
target_link_libraries(containers_flex_array_unit_tests
    PRIVATE
        containers              # tested target
        lcf_test_common         # shared test fixtures
        GTest::gtest
        GTest::gtest_main
)
gtest_discover_tests(containers_flex_array_unit_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES TIMEOUT 30
)

# ============================================================
# Performance benchmarks (NOT registered with CTest)
# Executable: containers_flex_array_bench
#
# Driven by google-benchmark with adaptive iteration counts. Output is
# routed through the project's spdlog-based Logger (lcf::Logger).
#
# Run manually:
#   .\containers_flex_array_bench.exe                        (Windows)
#   ./containers_flex_array_bench                            (Linux)
#
# Useful flags:
#   --benchmark_filter=PushBack.*       Run a subset.
#   --benchmark_min_time=0.5s           Min measurement time per case.
#   --benchmark_repetitions=5           Median + stddev across N runs.
#   --benchmark_format=json --benchmark_out=results.json
# ============================================================
if(LCF_BUILD_BENCHMARKS)
    add_executable(containers_flex_array_bench
        bench/benchmark.cpp
    )
    target_compile_features(containers_flex_array_bench PRIVATE cxx_std_23)
    target_link_libraries(containers_flex_array_bench
        PRIVATE
            containers
            utilities             # provides lcf::Logger / lcf_log_*
            benchmark::benchmark
    )
endif()
