set(LCF_ASYNC_BACKEND_BENCHMARK_RESULTS_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results" CACHE PATH "Backend comparison results")
if(NOT LCF_ASYNC_BACKEND_BENCHMARKS_IN_MAIN_BUILD)
    include(cmake/deferred.cmake)
    return()
endif()

include(cmake/dependencies.cmake)
set(CMAKE_CXX_SCAN_FOR_MODULES ${LCF_ASYNC_USE_MODULES})
add_executable(async_comparison_report EXCLUDE_FROM_ALL report.cpp)
target_compile_features(async_comparison_report PRIVATE cxx_std_23)
set_target_properties(async_comparison_report PROPERTIES CXX_EXTENSIONS OFF CXX_SCAN_FOR_MODULES OFF FOLDER "async/tests/benchmark/report")
add_test(NAME async.benchmark_report
    COMMAND "${CMAKE_COMMAND}" "-DREPORT_EXECUTABLE=$<TARGET_FILE:async_comparison_report>"
        "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/report-tests/$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_report.cmake")
set_tests_properties(async.benchmark_report PROPERTIES LABELS "async;benchmark_report")
foreach(async_backend asio stdexec)
    set(async_common "${CMAKE_CURRENT_SOURCE_DIR}/../../${async_backend}/tests/common")
    add_executable(async_compare_${async_backend} EXCLUDE_FROM_ALL benchmark.cpp)
    target_link_libraries(async_compare_${async_backend} PRIVATE lcf::async_${async_backend} benchmark::benchmark)
    add_executable(async_diagnostics_${async_backend} EXCLUDE_FROM_ALL probe.cpp)
    add_executable(async_memory_${async_backend} EXCLUDE_FROM_ALL probe.cpp allocation_probe.cpp)
    target_compile_definitions(async_memory_${async_backend} PRIVATE LCF_ASYNC_MEASURE_ALLOCATIONS=1)
    foreach(async_kind compare diagnostics memory)
        set(async_target async_${async_kind}_${async_backend})
        target_link_libraries(${async_target} PRIVATE lcf::async_${async_backend})
        target_include_directories(${async_target} PRIVATE "${async_common}")
        target_compile_definitions(${async_target} PRIVATE LCF_ASYNC_COMPARISON_BACKEND="${async_backend}")
        set_target_properties(${async_target} PROPERTIES CXX_EXTENSIONS OFF FOLDER "async/tests/benchmark/${async_backend}")
    endforeach()
endforeach()
add_custom_target(async_compare
    COMMAND "${CMAKE_COMMAND}" "-DRESULTS_DIR=${LCF_ASYNC_BACKEND_BENCHMARK_RESULTS_DIRECTORY}/$<CONFIG>"
        "-DREPORT_EXECUTABLE=$<TARGET_FILE:async_comparison_report>"
        "-DASIO_BENCHMARK=$<TARGET_FILE:async_compare_asio>" "-DSTDEXEC_BENCHMARK=$<TARGET_FILE:async_compare_stdexec>"
        "-DASIO_DIAGNOSTICS=$<TARGET_FILE:async_diagnostics_asio>" "-DSTDEXEC_DIAGNOSTICS=$<TARGET_FILE:async_diagnostics_stdexec>"
        "-DASIO_MEMORY=$<TARGET_FILE:async_memory_asio>" "-DSTDEXEC_MEMORY=$<TARGET_FILE:async_memory_stdexec>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run.cmake"
    DEPENDS async_comparison_report async_compare_asio async_compare_stdexec async_diagnostics_asio async_diagnostics_stdexec async_memory_asio async_memory_stdexec
    USES_TERMINAL VERBATIM)
set_target_properties(async_compare PROPERTIES FOLDER "async/tests/benchmark")
