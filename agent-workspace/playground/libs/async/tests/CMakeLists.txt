include(CMakeDependentOption)
cmake_dependent_option(LCF_ASYNC_BUILD_BACKEND_BENCHMARKS "Provide comparative backend benchmarks" OFF
    "LCF_ASYNC_BUILD_TESTS" OFF)
cmake_dependent_option(LCF_ASYNC_BACKEND_BENCHMARKS_IN_MAIN_BUILD "Configure comparative executables in this build" OFF
    "LCF_ASYNC_BUILD_BACKEND_BENCHMARKS" OFF)
if(LCF_ASYNC_BUILD_BACKEND_BENCHMARKS)
    if(NOT LCF_ASYNC_ENABLE_ASIO OR NOT LCF_ASYNC_ENABLE_STDEXEC)
        message(FATAL_ERROR "Backend comparisons require both LCF_ASYNC_ENABLE_ASIO and LCF_ASYNC_ENABLE_STDEXEC")
    endif()
    add_subdirectory(benchmark)
endif()
