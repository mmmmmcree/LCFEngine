cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LCFAsyncAsio LANGUAGES CXX)
endif()

option(LCF_ASYNC_USE_MODULES "Use the lcf.async module through async/task.h" OFF)
option(LCF_ASYNC_BUILD_VERIFY "Provide the async verification target" ON)
option(LCF_ASYNC_BUILD_BENCHMARKS "Provide the async benchmark target" ON)
option(LCF_ASYNC_TESTS_IN_MAIN_BUILD "Configure test executables in this build instead of on demand" OFF)
mark_as_advanced(LCF_ASYNC_TESTS_IN_MAIN_BUILD)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND LCF_ASYNC_BUILD_VERIFY)
    enable_testing()
endif()

include(cmake/fetch_deps.cmake)

if(NOT TARGET asio::asio)
    if(FETCHCONTENT_SOURCE_DIR_ASIO AND NOT EXISTS "${asio_SOURCE_DIR}/asio/include/asio.hpp")
        message(FATAL_ERROR "FETCHCONTENT_SOURCE_DIR_ASIO must contain asio/include/asio.hpp")
    endif()
    find_package(Threads REQUIRED)
    add_library(async_asio_dependency INTERFACE)
    add_library(asio::asio ALIAS async_asio_dependency)
    add_dependencies(async_asio_dependency async_asio_download)
    set_target_properties(async_asio_dependency PROPERTIES FOLDER "async/dependencies")

    target_include_directories(async_asio_dependency SYSTEM INTERFACE "${asio_SOURCE_DIR}/asio/include")
    target_compile_definitions(async_asio_dependency INTERFACE ASIO_STANDALONE)
    target_link_libraries(async_asio_dependency INTERFACE Threads::Threads)
    if(WIN32)
        target_compile_definitions(async_asio_dependency INTERFACE _WIN32_WINNT=0x0601)
        target_link_libraries(async_asio_dependency INTERFACE ws2_32 mswsock)
    endif()
endif()

set(async_header_sources
    include/async/task.h include/async/task/common.h
    include/async/task/Chain.h include/async/task/Flow.h include/async/task/interop.h)

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
    add_library(async_module_compile OBJECT EXCLUDE_FROM_ALL tests/compile/compile.cpp)
    set(async_compile_checks async_module_compile)
    add_custom_target(async_compile DEPENDS "$<TARGET_FILE:async_asio>" async_module_compile)
    set_target_properties(async_compile PROPERTIES FOLDER "async/tests/compile")
else()
    set(async_implementation_sources ${async_header_sources})
    add_library(async_asio INTERFACE)
    target_compile_features(async_asio INTERFACE cxx_std_23)
    target_include_directories(async_asio INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
    target_link_libraries(async_asio INTERFACE asio::asio)
    add_library(async_compile OBJECT EXCLUDE_FROM_ALL tests/compile/compile.cpp)
    set(async_compile_checks async_compile)
endif()
add_library(lcf::async_asio ALIAS async_asio)
set_target_properties(async_asio PROPERTIES FOLDER "async")
list(TRANSFORM async_implementation_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

target_link_libraries(${async_compile_checks} PRIVATE lcf::async_asio)
set_target_properties(${async_compile_checks} PROPERTIES
    CXX_EXTENSIONS OFF FOLDER "async/tests/compile")

if(LCF_ASYNC_BUILD_VERIFY OR LCF_ASYNC_BUILD_BENCHMARKS)
    add_subdirectory(tests)
endif()
