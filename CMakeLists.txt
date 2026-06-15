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

if(LCF_VKC_ONLY)
    set(LCF_SUBDIRS
        utilities/core
        utilities/enums
        utilities/log
        libs/vk_core
        examples/vkc_test)
elseif(LCF_TESTS_ONLY)
    set(LCF_SUBDIRS
        utilities
        containers)
else()
    set(LCF_SUBDIRS
        utilities
        containers
        profiling
        input
        GUI
        libs
        RenderEngine
        examples
        core
        Modules
        Systems)
endif()

if(LCF_BUILD_TESTS)
    list(APPEND LCF_SUBDIRS tests)
endif()

foreach(_dir IN LISTS LCF_SUBDIRS)
    add_pending_subdir(${_dir})
endforeach()

install_dependencies()

if(LCF_VKC_ONLY)
    include(window_config)
elseif(NOT LCF_TESTS_ONLY)
    include(window_config)
    include(profiling_config)
endif()

resolve_pending_subdirs()
