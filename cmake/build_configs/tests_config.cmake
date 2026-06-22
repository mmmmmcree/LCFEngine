# ============================================================
# tests_config.cmake — option-only stage.
#
# Declares the LCF_BUILD_TESTS / LCF_BUILD_BENCHMARKS / LCF_TESTS_ONLY
# switches. Safe to include before install_dependencies() — does NOT call
# find_package. The find_package(GTest) / find_package(benchmark) calls
# live in tests_config_resolve.cmake, which the top-level CMakeLists
# includes after install_dependencies() has populated CMAKE_PREFIX_PATH.
#
# Three switches:
#   LCF_BUILD_TESTS       Build unit tests (gtest).
#   LCF_BUILD_BENCHMARKS  Build performance benchmarks (google-benchmark).
#                         Requires LCF_BUILD_TESTS=ON.
#   LCF_TESTS_ONLY        Skip heavyweight non-test modules so the test
#                         build is minimal.
# ============================================================

option(LCF_BUILD_TESTS "Build LCFEngine unit tests" OFF)
option(LCF_BUILD_BENCHMARKS "Build LCFEngine performance benchmarks (requires LCF_BUILD_TESTS)" OFF)
option(LCF_TESTS_ONLY "Skip non-test modules (RenderEngine/libs/examples/...) for a minimal test build" OFF)
option(LCF_VKC_ONLY "Build only vk_core and its runnable chain (GUI/core/input/math + vkc_examples) for fast vk_core iteration" OFF)
