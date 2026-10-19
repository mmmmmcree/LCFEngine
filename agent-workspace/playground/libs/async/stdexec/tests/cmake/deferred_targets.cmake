include(ExternalProject)
include("${CMAKE_CURRENT_LIST_DIR}/../../../tests/verify/interface_tests.cmake")
if(LCF_ASYNC_DEPENDENCY_PROVIDER STREQUAL "PROVIDED"
    OR LCF_ASYNC_STDEXEC_GTEST_PROVIDER STREQUAL "PROVIDED"
    OR LCF_ASYNC_STDEXEC_BENCHMARK_PROVIDER STREQUAL "PROVIDED")
    message(FATAL_ERROR "PROVIDED targets cannot cross build trees; set LCF_ASYNC_STDEXEC_TESTS_IN_MAIN_BUILD=ON")
endif()
set(async_test_args -DLCF_ASYNC_BUILD_TESTS:BOOL=ON
    -DLCF_ASYNC_STDEXEC_TESTS_IN_MAIN_BUILD:BOOL=ON
    -DLCF_ASYNC_INSTALL:BOOL=OFF)
get_cmake_property(async_variables VARIABLES)
foreach(async_variable IN LISTS async_variables)
    if(async_variable MATCHES "^(LCF_ASYNC_.*PROVIDER|LCF_ASYNC_USE_MODULES|FETCHCONTENT_SOURCE_DIR_.*|FETCHCONTENT_(FULLY|UPDATES)_DISCONNECTED|CMAKE_(CXX_COMPILER|CXX_COMPILER_LAUNCHER|MAKE_PROGRAM|TOOLCHAIN_FILE|BUILD_TYPE|CONFIGURATION_TYPES|MSVC_RUNTIME_LIBRARY|CXX_FLAGS.*|EXE_LINKER_FLAGS.*|PREFIX_PATH|FIND_ROOT_PATH|SYSROOT|OSX_.*))$")
        string(REPLACE ";" "|" async_value "${${async_variable}}")
        list(APPEND async_test_args "-D${async_variable}:STRING=${async_value}")
    endif()
endforeach()
foreach(async_operation verify sandbox benchmark)
    if(async_operation STREQUAL "benchmark")
        set(async_option BENCHMARKS)
    else()
        string(TOUPPER "${async_operation}" async_option)
    endif()
    if(NOT LCF_ASYNC_STDEXEC_BUILD_${async_option})
        continue()
    endif()
    set(async_component_args)
    foreach(async_component VERIFY SANDBOX BENCHMARKS)
        if(async_component STREQUAL async_option)
            list(APPEND async_component_args -DLCF_ASYNC_STDEXEC_BUILD_${async_component}:BOOL=ON)
        else()
            list(APPEND async_component_args -DLCF_ASYNC_STDEXEC_BUILD_${async_component}:BOOL=OFF)
        endif()
    endforeach()
    ExternalProject_Add(async_stdexec_${async_operation}_project
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.."
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/${async_operation}-build"
        PREFIX "${CMAKE_CURRENT_BINARY_DIR}/${async_operation}-project"
        DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
        CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
        CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}"
        LIST_SEPARATOR |
        CMAKE_ARGS ${async_test_args} ${async_component_args}
        CONFIGURE_HANDLED_BY_BUILD TRUE
        BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config "$<CONFIG>" --target async_stdexec_${async_operation}
        INSTALL_COMMAND "" BUILD_ALWAYS TRUE EXCLUDE_FROM_ALL TRUE
        USES_TERMINAL_CONFIGURE TRUE USES_TERMINAL_BUILD TRUE)
    add_custom_target(async_stdexec_${async_operation} DEPENDS async_stdexec_${async_operation}_project)
    target_sources(async_stdexec_${async_operation} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${async_operation}/CMakeLists.txt")
    if(async_operation STREQUAL "verify")
        async_add_interface_tests(async_stdexec_${async_operation})
        target_sources(async_stdexec_${async_operation} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/verify/native_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/verify/poll_test.cpp")
    elseif(async_operation STREQUAL "sandbox")
        target_sources(async_stdexec_${async_operation} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/sandbox.cpp")
    else()
        target_sources(async_stdexec_${async_operation} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/benchmark.cpp")
    endif()
    set_target_properties(async_stdexec_${async_operation} PROPERTIES FOLDER "async/stdexec/tests/${async_operation}")
    set_target_properties(async_stdexec_${async_operation}_project PROPERTIES FOLDER "async/stdexec/tests/${async_operation}/internal")
    if(NOT async_operation STREQUAL "benchmark")
        add_test(NAME stdexec.${async_operation}
            COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target async_stdexec_${async_operation})
        set_tests_properties(stdexec.${async_operation} PROPERTIES LABELS "async;stdexec;deferred" RUN_SERIAL TRUE)
    endif()
endforeach()
