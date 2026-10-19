cmake_minimum_required(VERSION 3.28)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LCFAsyncStdexec VERSION 0.1.0 LANGUAGES CXX)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT EXISTS "${CMAKE_BINARY_DIR}/.gitignore")
        file(WRITE "${CMAKE_BINARY_DIR}/.gitignore" "*\n")
    endif()
endif()

include(../cmake/options.cmake)
include(cmake/options.cmake)
set(CMAKE_CXX_SCAN_FOR_MODULES ${LCF_ASYNC_USE_MODULES})
if(LCF_ASYNC_STDEXEC_BUILD_TESTS)
    enable_testing()
endif()
include(cmake/fetch_deps.cmake)
include(../cmake/dependency_targets.cmake)
async_dependency_targets(stdexec "${CMAKE_CURRENT_SOURCE_DIR}/deps.json")
include(GNUInstallDirs)

if(LCF_ASYNC_USE_MODULES)
    if(NOT CMAKE_GENERATOR MATCHES "Ninja|Visual Studio")
        message(FATAL_ERROR "lcf.async modules require a generator with CXX_MODULES support")
    endif()
    add_library(async_stdexec STATIC)
    target_sources(async_stdexec PUBLIC FILE_SET CXX_MODULES FILES modules/async.cppm)
    target_compile_definitions(async_stdexec INTERFACE LCF_ASYNC_USE_MODULES=1)
    set(async_scope PUBLIC)
else()
    add_library(async_stdexec INTERFACE)
    set(async_scope INTERFACE)
endif()
add_library(lcf::async_stdexec ALIAS async_stdexec)
target_compile_features(async_stdexec ${async_scope} cxx_std_23)
if(MSVC)
    target_compile_options(async_stdexec ${async_scope} /Zc:__cplusplus /Zc:preprocessor /permissive- /bigobj)
endif()
target_include_directories(async_stdexec ${async_scope}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lcf_async/stdexec>")
target_link_libraries(async_stdexec ${async_scope} STDEXEC::stdexec Threads::Threads)
set_target_properties(async_stdexec PROPERTIES FOLDER "async/stdexec" CXX_EXTENSIONS OFF)
set(async_header_sources
    include/async/config.h
    include/async/core.h
    include/async/core/Awaitable.h
    include/async/core/Context.h
    include/async/core/Executor.h
    include/async/core/Signal.h
    include/async/core/cancellation.h
    include/async/core/operations.h
    include/async/core/timer.h
    include/async/dependencies.h
    include/async/execution.h
    include/async/execution/cancellation/details/StopRegistrationGate.h
    include/async/execution/cancellation/operations.h
    include/async/execution/coroutine/Awaitable.h
    include/async/execution/coroutine/FrameAllocatorScope.h
    include/async/execution/coroutine/details/awaitable.h
    include/async/execution/coroutine/details/frame.h
    include/async/execution/coroutine/details/traits.h
    include/async/execution/coroutine/operations.h
    include/async/execution/scheduling/Context.h
    include/async/execution/scheduling/Executor.h
    include/async/execution/scheduling/QueueScheduler.h
    include/async/execution/scheduling/details/queue.h
    include/async/execution/stdexec.h
    include/async/execution/synchronization/Signal.h
    include/async/execution/synchronization/details/notification.h
    include/async/execution/timer/details/timer.h
    include/async/execution/timer/details/timer_wait.h
    include/async/execution/timer/operations.h
    include/async/implementation.h
    include/async/task.h
    include/async/task/Chain.h
    include/async/task/ChainJunction.h
    include/async/task/chain_details.h
    include/async/task/common.h
    include/async/task/error_handling.h
    include/async/task/execution.h
    include/async/task/notification.h)
target_sources(async_stdexec PRIVATE ${async_header_sources})
list(TRANSFORM async_header_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${async_header_sources})

if(LCF_ASYNC_STDEXEC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(LCF_ASYNC_INSTALL)
    include(cmake/install.cmake)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        include(../cmake/install.cmake)
    endif()
endif()
