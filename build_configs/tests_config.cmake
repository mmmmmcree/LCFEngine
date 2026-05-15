# ============================================================
# tests_config.cmake
# Opt-in test infrastructure for LCFEngine.
#
# Three switches:
#   LCF_BUILD_TESTS       Build unit tests (gtest).
#   LCF_BUILD_BENCHMARKS  Build performance benchmarks (google-benchmark).
#                         Requires LCF_BUILD_TESTS=ON.
#   LCF_TESTS_ONLY        Skip heavyweight non-test modules (RenderEngine,
#                         libs, examples, ...) so the test build is minimal.
#                         Use the corresponding CMakePresets entries.
# ============================================================

option(LCF_BUILD_TESTS "Build LCFEngine unit tests" OFF)
option(LCF_BUILD_BENCHMARKS "Build LCFEngine performance benchmarks (requires LCF_BUILD_TESTS)" OFF)
option(LCF_TESTS_ONLY "Skip non-test modules (RenderEngine/libs/examples/...) for a minimal test build" OFF)

if(NOT LCF_BUILD_TESTS)
    return()
endif()

# CTest infrastructure
include(CTest)

# GoogleTest (always required when tests are enabled).
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

if(NOT TARGET GTest::gtest)
    message(FATAL_ERROR
        "LCF_BUILD_TESTS=ON but GTest::gtest target was not found.\n"
        "Make sure the vcpkg 'tests' feature is enabled:\n"
        "  cmake -S . -B build -DLCF_BUILD_TESTS=ON -DVCPKG_MANIFEST_FEATURES=tests"
    )
endif()

# google-benchmark (only when benchmarks are enabled).
if(LCF_BUILD_BENCHMARKS)
    find_package(benchmark CONFIG REQUIRED)
    if(NOT TARGET benchmark::benchmark)
        message(FATAL_ERROR
            "LCF_BUILD_BENCHMARKS=ON but benchmark::benchmark target was not found.\n"
            "Make sure the vcpkg 'tests' feature is enabled."
        )
    endif()
endif()
