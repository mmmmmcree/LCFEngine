include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if(LCF_ASYNC_BUILD_VERIFY AND NOT TARGET GTest::gtest_main)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    # GoogleTest 1.17.0.
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(LCF_ASYNC_BUILD_BENCHMARKS AND NOT TARGET benchmark::benchmark)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    # Google Benchmark 1.9.5.
    FetchContent_Declare(googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG 192ef10025eb2c4cdd392bc502f0c852196baa48
    )
    FetchContent_MakeAvailable(googlebenchmark)
endif()
