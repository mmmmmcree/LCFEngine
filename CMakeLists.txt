cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH
     "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts"
     "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_configs")
include(deps)

project(LCFEngine LANGUAGES CXX)

include(global_config)
include(graphic_api_config)
include(tests_config)

find_package(Threads)

add_pending_subdir(utilities)
add_pending_subdir(containers)

if(NOT LCF_TESTS_ONLY)
    add_pending_subdir(profiling)
    add_pending_subdir(input)
    add_pending_subdir(GUI)
    add_pending_subdir(libs)
    add_pending_subdir(RenderEngine)
    add_pending_subdir(examples)
    add_pending_subdir(core)
    add_pending_subdir(Modules)
    add_pending_subdir(Systems)
endif()

if(LCF_BUILD_TESTS)
    add_pending_subdir(tests)
endif()

install_dependencies()

if(NOT LCF_TESTS_ONLY)
    include(window_config)
    include(profiling_config)
endif()

resolve_pending_subdirs()
