include(ExternalProject)
include("${CMAKE_CURRENT_LIST_DIR}/../../../tests/verify/interface_tests.cmake")

# The outer project exposes entry targets without configuring the test frameworks.
# Each requested target configures, builds and runs only its own test project.
if(LCF_ASYNC_DEPENDENCY_PROVIDER STREQUAL "PROVIDED"
    OR LCF_ASYNC_ASIO_PROVIDER STREQUAL "PROVIDED"
    OR LCF_ASYNC_ASIO_GTEST_PROVIDER STREQUAL "PROVIDED"
    OR LCF_ASYNC_ASIO_BENCHMARK_PROVIDER STREQUAL "PROVIDED")
    message(FATAL_ERROR "PROVIDED targets cannot cross build trees; set LCF_ASYNC_ASIO_TESTS_IN_MAIN_BUILD=ON")
endif()
set(async_test_cmake_args
    -DLCF_ASYNC_BUILD_TESTS:BOOL=ON
    -DLCF_ASYNC_INSTALL:BOOL=OFF
    -DLCF_ASYNC_ASIO_TESTS_IN_MAIN_BUILD:BOOL=ON
    -DLCF_ASYNC_USE_MODULES:BOOL=${LCF_ASYNC_USE_MODULES}
)
foreach(async_variable
    LCF_ASYNC_DEPENDENCY_PROVIDER LCF_ASYNC_ASIO_PROVIDER
    LCF_ASYNC_ASIO_GTEST_PROVIDER LCF_ASYNC_ASIO_BENCHMARK_PROVIDER
    CMAKE_PREFIX_PATH CMAKE_FIND_ROOT_PATH FETCHCONTENT_SOURCE_DIR_ASIO
    CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_LAUNCHER CMAKE_MAKE_PROGRAM
    CMAKE_TOOLCHAIN_FILE CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES
    CMAKE_MSVC_RUNTIME_LIBRARY CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS
    CMAKE_SYSROOT CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES
    CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_EXPORT_COMPILE_COMMANDS
    FETCHCONTENT_SOURCE_DIR_GOOGLETEST FETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK
    FETCHCONTENT_FULLY_DISCONNECTED FETCHCONTENT_UPDATES_DISCONNECTED)
    if(DEFINED ${async_variable})
        string(REPLACE ";" "|" async_value "${${async_variable}}")
        list(APPEND async_test_cmake_args "-D${async_variable}:STRING=${async_value}")
    endif()
endforeach()
foreach(async_configuration IN LISTS CMAKE_CONFIGURATION_TYPES ITEMS "${CMAKE_BUILD_TYPE}")
    string(TOUPPER "${async_configuration}" async_configuration_upper)
    foreach(async_prefix CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS)
        set(async_variable "${async_prefix}_${async_configuration_upper}")
        if(DEFINED ${async_variable})
            list(APPEND async_test_cmake_args "-D${async_variable}:STRING=${${async_variable}}")
        endif()
    endforeach()
endforeach()
if(asio_SOURCE_DIR)
    list(APPEND async_test_cmake_args "-DFETCHCONTENT_SOURCE_DIR_ASIO:PATH=${asio_SOURCE_DIR}")
endif()
set(async_test_dependencies "")
if(TARGET async_asio_download)
    list(APPEND async_test_dependencies async_asio_download)
endif()

foreach(async_operation verify sandbox benchmark)
    if(async_operation STREQUAL "verify")
        if(NOT LCF_ASYNC_ASIO_BUILD_VERIFY)
            continue()
        endif()
        set(async_operation_args -DLCF_ASYNC_ASIO_BUILD_VERIFY:BOOL=ON
            -DLCF_ASYNC_ASIO_BUILD_SANDBOX:BOOL=OFF -DLCF_ASYNC_ASIO_BUILD_BENCHMARKS:BOOL=OFF)
    elseif(async_operation STREQUAL "sandbox")
        if(NOT LCF_ASYNC_ASIO_BUILD_SANDBOX)
            continue()
        endif()
        set(async_operation_args -DLCF_ASYNC_ASIO_BUILD_VERIFY:BOOL=OFF
            -DLCF_ASYNC_ASIO_BUILD_SANDBOX:BOOL=ON -DLCF_ASYNC_ASIO_BUILD_BENCHMARKS:BOOL=OFF)
    else()
        if(NOT LCF_ASYNC_ASIO_BUILD_BENCHMARKS)
            continue()
        endif()
        set(async_operation_args -DLCF_ASYNC_ASIO_BUILD_VERIFY:BOOL=OFF
            -DLCF_ASYNC_ASIO_BUILD_SANDBOX:BOOL=OFF -DLCF_ASYNC_ASIO_BUILD_BENCHMARKS:BOOL=ON
            "-DLCF_ASYNC_ASIO_BENCHMARK_RESULTS_DIRECTORY:PATH=${CMAKE_CURRENT_BINARY_DIR}/benchmark/results")
    endif()

    ExternalProject_Add(async_${async_operation}_project
        PREFIX "${CMAKE_CURRENT_BINARY_DIR}/${async_operation}-project"
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.."
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/${async_operation}-build"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
        CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
        CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}"
        CMAKE_GENERATOR_INSTANCE "${CMAKE_GENERATOR_INSTANCE}"
        LIST_SEPARATOR |
        CMAKE_ARGS ${async_test_cmake_args} ${async_operation_args}
        DEPENDS ${async_test_dependencies}
        CONFIGURE_HANDLED_BY_BUILD TRUE
        BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config "$<CONFIG>" --target async_${async_operation}
        INSTALL_COMMAND ""
        BUILD_ALWAYS TRUE
        EXCLUDE_FROM_ALL TRUE
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
    )
    add_custom_target(async_${async_operation} DEPENDS async_${async_operation}_project)
    target_sources(async_${async_operation} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${async_operation}/CMakeLists.txt")
    if(async_operation STREQUAL "verify")
        async_add_interface_tests(async_${async_operation})
    elseif(async_operation STREQUAL "sandbox")
        target_sources(async_${async_operation} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/chain_sandbox.cpp")
    else()
        target_sources(async_${async_operation} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/benchmark.cpp")
    endif()
    set_target_properties(async_${async_operation} PROPERTIES FOLDER "async/asio/tests/${async_operation}")
    set_target_properties(async_${async_operation}_project PROPERTIES FOLDER "async/asio/tests/${async_operation}/internal")
endforeach()

set(async_ctest_operations "")
if(LCF_ASYNC_ASIO_BUILD_VERIFY)
    list(APPEND async_ctest_operations verify)
endif()
if(LCF_ASYNC_ASIO_BUILD_SANDBOX)
    list(APPEND async_ctest_operations sandbox)
endif()
if(async_ctest_operations)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/deferred_ctest.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/deferred_ctest.cmake" @ONLY)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
        "${CMAKE_CURRENT_BINARY_DIR}/deferred_ctest.cmake")
endif()
