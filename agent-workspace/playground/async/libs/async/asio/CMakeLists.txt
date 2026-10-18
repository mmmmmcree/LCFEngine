cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LCFAsyncAsio LANGUAGES CXX)
endif()

option(LCF_ASYNC_USE_MODULES "Use the lcf.async module through async/task.h" OFF)
option(LCF_ASYNC_BUILD_VERIFY "Provide the async verification target" ON)
option(LCF_ASYNC_BUILD_SANDBOX "Provide the async sandbox target" ON)
option(LCF_ASYNC_BUILD_BENCHMARKS "Provide the async benchmark target" ON)
option(LCF_ASYNC_TESTS_IN_MAIN_BUILD "Configure test executables in this build instead of on demand" OFF)
mark_as_advanced(LCF_ASYNC_TESTS_IN_MAIN_BUILD)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND (LCF_ASYNC_BUILD_VERIFY OR LCF_ASYNC_BUILD_SANDBOX))
    enable_testing()
endif()

include(cmake/fetch_deps.cmake)

if(NOT TARGET list_dependency_versions)
    add_custom_target(list_dependency_versions
        COMMAND "${CMAKE_COMMAND}" "-DDEPS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/deps.json"
            "-DCACHE_DIR=${CMAKE_BINARY_DIR}/_deps/dependency-versions"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependency_versions.cmake"
        COMMENT "List recent dependency tags by Git timestamp"
        USES_TERMINAL
        VERBATIM)
    set_target_properties(list_dependency_versions PROPERTIES FOLDER "async/dependencies")
endif()
if(NOT TARGET update_deps_to_latest)
    add_custom_target(update_deps_to_latest
        COMMAND "${CMAKE_COMMAND}" "-DDEPS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/deps.json" -DAPPLY=ON
            "-DCACHE_DIR=${CMAKE_BINARY_DIR}/_deps/dependency-versions"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependency_versions.cmake"
        COMMENT "Pin deps.json to the newest tag by Git timestamp"
        USES_TERMINAL
        VERBATIM)
    set_target_properties(update_deps_to_latest PROPERTIES FOLDER "async/dependencies")
endif()

if(NOT TARGET asio::asio)
    if(FETCHCONTENT_SOURCE_DIR_ASIO AND NOT EXISTS "${asio_SOURCE_DIR}/include/asio.hpp"
        AND NOT EXISTS "${asio_SOURCE_DIR}/asio/include/asio.hpp")
        message(FATAL_ERROR "FETCHCONTENT_SOURCE_DIR_ASIO must contain include/asio.hpp or asio/include/asio.hpp")
    endif()
    find_package(Threads REQUIRED)
    add_library(async_asio_dependency INTERFACE)
    add_library(asio::asio ALIAS async_asio_dependency)
    add_dependencies(async_asio_dependency async_asio_download)
    set_target_properties(async_asio_dependency PROPERTIES FOLDER "async/dependencies")

    target_include_directories(async_asio_dependency SYSTEM INTERFACE
        "${asio_SOURCE_DIR}/include" "${asio_SOURCE_DIR}/asio/include")
    target_compile_definitions(async_asio_dependency INTERFACE ASIO_STANDALONE)
    target_link_libraries(async_asio_dependency INTERFACE Threads::Threads)
    if(WIN32)
        target_compile_definitions(async_asio_dependency INTERFACE _WIN32_WINNT=0x0601)
        target_link_libraries(async_asio_dependency INTERFACE ws2_32 mswsock)
    endif()
endif()

set(async_header_sources
    include/async/task.h include/async/task/common.h
    include/async/task/chain_details.h
    include/async/task/error_handling.h
    include/async/task/Chain.h include/async/task/ChainJunction.h)

if(LCF_ASYNC_USE_MODULES)
    # Keep Asio in the global module and compile its runtime once. This also
    # avoids serializing header-only services into the MSVC module interface.
    add_library(async_asio_runtime STATIC src/asio.cpp)
    target_link_libraries(async_asio_runtime PUBLIC asio::asio)
    target_compile_features(async_asio_runtime PUBLIC cxx_std_23)
    target_compile_definitions(async_asio_runtime PUBLIC ASIO_SEPARATE_COMPILATION)
    set_target_properties(async_asio_runtime PROPERTIES
        CXX_EXTENSIONS OFF FOLDER "async/dependencies")

    set(async_implementation_sources ${async_header_sources} modules/async.cppm src/asio.cpp)
    add_library(async_asio STATIC)
    target_sources(async_asio PUBLIC FILE_SET CXX_MODULES FILES modules/async.cppm)
    target_include_directories(async_asio PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
    target_compile_features(async_asio PUBLIC cxx_std_23)
    target_compile_definitions(async_asio INTERFACE LCF_ASYNC_USE_MODULES=1)
    target_link_libraries(async_asio PUBLIC async_asio_runtime)
    set_target_properties(async_asio PROPERTIES CXX_EXTENSIONS OFF)
else()
    set(async_implementation_sources ${async_header_sources})
    add_library(async_asio INTERFACE)
    target_compile_features(async_asio INTERFACE cxx_std_23)
    target_include_directories(async_asio INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
    target_link_libraries(async_asio INTERFACE asio::asio)
endif()
add_library(lcf::async_asio ALIAS async_asio)
set_target_properties(async_asio PROPERTIES FOLDER "async")
list(TRANSFORM async_implementation_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

if(LCF_ASYNC_BUILD_VERIFY OR LCF_ASYNC_BUILD_SANDBOX OR LCF_ASYNC_BUILD_BENCHMARKS)
    add_subdirectory(tests)
endif()
