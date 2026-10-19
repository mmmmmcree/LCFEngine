add_executable(async_benchmark_runner benchmark.cpp)
target_link_libraries(async_benchmark_runner PRIVATE lcf::async_asio benchmark::benchmark)
set_target_properties(async_benchmark_runner PROPERTIES
    CXX_EXTENSIONS OFF FOLDER "async/asio/tests/benchmark")
target_compile_definitions(async_benchmark_runner PRIVATE
    LCF_ASYNC_BENCHMARK_CONFIGURATION="$<CONFIG>"
)

set(async_source_fingerprints "")
foreach(async_source IN LISTS async_implementation_sources)
    file(SHA256 "${async_source}" async_source_hash)
    string(APPEND async_source_fingerprints "${async_source_hash}")
endforeach()
string(SHA256 async_implementation_hash "${async_source_fingerprints}")
if(LCF_ASYNC_USE_MODULES)
    set(async_representation "module")
else()
    set(async_representation "header")
endif()
target_compile_definitions(async_benchmark_runner PRIVATE
    LCF_ASYNC_IMPLEMENTATION_HASH="${async_implementation_hash}"
    LCF_ASYNC_IMPLEMENTATION="${async_representation}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${async_implementation_sources})

if(NOT LCF_ASYNC_ASIO_BENCHMARK_RESULTS_DIRECTORY)
    set(LCF_ASYNC_ASIO_BENCHMARK_RESULTS_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results")
endif()
set(async_benchmark_results "${LCF_ASYNC_ASIO_BENCHMARK_RESULTS_DIRECTORY}/$<CONFIG>")
add_custom_target(async_benchmark
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${async_benchmark_results}"
    COMMAND "$<TARGET_FILE:async_benchmark_runner>"
        --benchmark_min_time=0.05s
        --benchmark_repetitions=3
        --benchmark_report_aggregates_only=true
        "--benchmark_out=${async_benchmark_results}/latest.json"
        --benchmark_out_format=json
    DEPENDS async_benchmark_runner
    USES_TERMINAL
    COMMENT "Running async benchmarks and writing results/$<CONFIG>/latest.json"
)
set_target_properties(async_benchmark PROPERTIES FOLDER "async/asio/tests/benchmark")
