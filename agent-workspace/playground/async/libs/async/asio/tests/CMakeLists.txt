if(NOT LCF_ASYNC_TESTS_IN_MAIN_BUILD)
    include(cmake/deferred_targets.cmake)
    return()
endif()

include(cmake/fetch_test_tools.cmake)

if(LCF_ASYNC_BUILD_VERIFY)
    add_subdirectory(verify)
endif()

if(LCF_ASYNC_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
