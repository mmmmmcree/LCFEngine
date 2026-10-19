async_stdexec_test_dependency(BENCHMARK benchmark::benchmark benchmark googlebenchmark
    https://github.com/google/benchmark.git 192ef10025eb2c4cdd392bc502f0c852196baa48)
add_executable(async_stdexec_benchmark_tests benchmark.cpp)
target_link_libraries(async_stdexec_benchmark_tests PRIVATE lcf::async_stdexec benchmark::benchmark)
set(LCF_ASYNC_STDEXEC_BENCHMARK_RESULTS_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results" CACHE PATH "Benchmark JSON output directory")
add_custom_target(async_stdexec_benchmark
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${LCF_ASYNC_STDEXEC_BENCHMARK_RESULTS_DIRECTORY}"
    COMMAND $<TARGET_FILE:async_stdexec_benchmark_tests> --benchmark_min_time=0.1s
        "--benchmark_out=${LCF_ASYNC_STDEXEC_BENCHMARK_RESULTS_DIRECTORY}/latest.json" --benchmark_out_format=json
    DEPENDS async_stdexec_benchmark_tests USES_TERMINAL)
set_target_properties(async_stdexec_benchmark async_stdexec_benchmark_tests PROPERTIES FOLDER "async/stdexec/tests/benchmark")
