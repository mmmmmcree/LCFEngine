cmake_minimum_required(VERSION 3.16)

project(LCFEngine LANGUAGES CXX)

option(LCF_DEPLOY "Deploy mode: bundle assets next to executable" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_configs")
set(CONFIG_MODULES
    global_config
    graphic_api_config
    window_config
    profiling_config
    tests_config
)
foreach(module ${CONFIG_MODULES})
    include(${module})
endforeach()

# ============================================================
# Module subdirectories.
#
# When LCF_TESTS_ONLY=ON, we skip heavyweight modules (RenderEngine,
# libs, examples, ...) and only configure the minimum needed by the
# test executables. This makes test-only builds dramatically faster
# and avoids pulling in unrelated vcpkg dependencies (vulkan, sdl3,
# assimp, etc.) when only running ctest.
#
# Modules required by tests/:
#   - utilities  (Logger / spdlog / lcf_log_*)
#   - containers (target under test)
# ============================================================
add_subdirectory(utilities)
add_subdirectory(containers)

if(NOT LCF_TESTS_ONLY)
    add_subdirectory(profiling)
    add_subdirectory(input)
    add_subdirectory(GUI)
    add_subdirectory(libs)
    add_subdirectory(RenderEngine)
    add_subdirectory(examples)
    add_subdirectory(core)
    add_subdirectory(Modules)
    add_subdirectory(Systems)
endif()

if(LCF_BUILD_TESTS)
    add_subdirectory(tests)
endif()
