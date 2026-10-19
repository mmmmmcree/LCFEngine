cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LCFAsyncAsio LANGUAGES CXX)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT EXISTS "${CMAKE_BINARY_DIR}/.gitignore")
        file(WRITE "${CMAKE_BINARY_DIR}/.gitignore" "*\n")
    endif()
endif()

include(../cmake/options.cmake)
include(cmake/options.cmake)
set(CMAKE_CXX_SCAN_FOR_MODULES ${LCF_ASYNC_USE_MODULES})
include(GNUInstallDirs)
if(LCF_ASYNC_ASIO_BUILD_TESTS)
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
    set_target_properties(list_dependency_versions PROPERTIES FOLDER "async/asio/dependencies")
endif()
if(NOT TARGET update_deps_to_latest)
    add_custom_target(update_deps_to_latest
        COMMAND "${CMAKE_COMMAND}" "-DDEPS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/deps.json" -DAPPLY=ON
            "-DCACHE_DIR=${CMAKE_BINARY_DIR}/_deps/dependency-versions"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependency_versions.cmake"
        COMMENT "Pin deps.json to the newest tag by Git timestamp"
        USES_TERMINAL
        VERBATIM)
    set_target_properties(update_deps_to_latest PROPERTIES FOLDER "async/asio/dependencies")
endif()

set(async_header_sources
    include/async/core.h
    include/async/core/Awaitable.h
    include/async/core/Context.h
    include/async/core/Executor.h
    include/async/core/Signal.h
    include/async/core/cancellation.h
    include/async/core/operations.h
    include/async/core/timer.h
    include/async/task.h
    include/async/task/Chain.h
    include/async/task/ChainJunction.h
    include/async/task/chain_details.h
    include/async/task/common.h
    include/async/task/error_handling.h)

if(LCF_ASYNC_USE_MODULES)
    # Keep Asio in the global module and compile its runtime once. This also
    # avoids serializing header-only services into the MSVC module interface.
    add_library(async_asio_runtime STATIC src/asio.cpp)
    target_link_libraries(async_asio_runtime PUBLIC asio::asio)
    target_compile_features(async_asio_runtime PUBLIC cxx_std_23)
    target_compile_definitions(async_asio_runtime PUBLIC ASIO_SEPARATE_COMPILATION)
    set_target_properties(async_asio_runtime PROPERTIES
        CXX_EXTENSIONS OFF FOLDER "async/asio/dependencies")

    set(async_implementation_sources ${async_header_sources} modules/async.cppm src/asio.cpp)
    add_library(async_asio STATIC)
    target_sources(async_asio PUBLIC FILE_SET CXX_MODULES FILES modules/async.cppm)
    target_include_directories(async_asio PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lcf_async/asio>")
    target_compile_features(async_asio PUBLIC cxx_std_23)
    target_compile_definitions(async_asio INTERFACE LCF_ASYNC_USE_MODULES=1)
    target_link_libraries(async_asio PUBLIC async_asio_runtime)
    set_target_properties(async_asio PROPERTIES CXX_EXTENSIONS OFF)
else()
    set(async_implementation_sources ${async_header_sources})
    add_library(async_asio INTERFACE)
    target_compile_features(async_asio INTERFACE cxx_std_23)
    target_include_directories(async_asio INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lcf_async/asio>")
    target_link_libraries(async_asio INTERFACE asio::asio)
endif()
add_library(lcf::async_asio ALIAS async_asio)
target_sources(async_asio PRIVATE ${async_header_sources})
set_target_properties(async_asio PROPERTIES FOLDER "async/asio")
list(TRANSFORM async_implementation_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${async_implementation_sources})

if(LCF_ASYNC_ASIO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(LCF_ASYNC_INSTALL)
    include(cmake/install.cmake)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        include(../cmake/install.cmake)
    endif()
endif()
