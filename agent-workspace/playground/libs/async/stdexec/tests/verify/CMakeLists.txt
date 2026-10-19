async_stdexec_test_dependency(GTEST GTest::gtest_main GTest googletest
    https://github.com/google/googletest.git 52eb8108c5bdec04579160ae17225d66034bd723)
include(../../../tests/verify/interface_tests.cmake)
add_executable(async_stdexec_verify_tests
    awaitable_test.cpp
    stop_registration_test.cpp
    native_test.cpp
    poll_test.cpp)
async_add_interface_tests(async_stdexec_verify_tests)
target_include_directories(async_stdexec_verify_tests PRIVATE ../common)
target_link_libraries(async_stdexec_verify_tests PRIVATE lcf::async_stdexec GTest::gtest_main)
target_compile_definitions(async_stdexec_verify_tests PRIVATE LCF_ASYNC_STDEXEC=1)
set_target_properties(async_stdexec_verify_tests PROPERTIES CXX_EXTENSIONS OFF FOLDER "async/stdexec/tests/verify")
include(GoogleTest)
if(LCF_ASYNC_USE_MODULES)
    target_sources(async_stdexec_verify_tests PRIVATE module_bridge.cpp module_test.cpp)
    add_library(async_stdexec_module_private_probe OBJECT EXCLUDE_FROM_ALL module_private_probe.cpp)
    target_link_libraries(async_stdexec_module_private_probe PRIVATE lcf::async_stdexec)
    set_target_properties(async_stdexec_module_private_probe PROPERTIES FOLDER "async/stdexec/tests/verify/internal")
    add_test(NAME stdexec.Modules.InternalNamesAreNotExported
        COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}" "-DCONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/check_module_privacy.cmake")
    set_tests_properties(stdexec.Modules.InternalNamesAreNotExported PROPERTIES LABELS stdexec_verify RUN_SERIAL TRUE TIMEOUT 60)
    foreach(probe IN ITEMS frame queue)
        set(probe_target "async_stdexec_module_${probe}_probe")
        string(TOUPPER "${probe}" probe_definition)
        if(probe STREQUAL "frame")
            set(private_name allocate_frame)
        else()
            set(private_name Queue)
        endif()
        add_library(${probe_target} OBJECT EXCLUDE_FROM_ALL module_private_probe.cpp)
        target_link_libraries(${probe_target} PRIVATE lcf::async_stdexec)
        target_compile_definitions(${probe_target} PRIVATE "LCF_ASYNC_PROBE_${probe_definition}=1")
        set_target_properties(${probe_target} PROPERTIES FOLDER "async/stdexec/tests/verify/internal")
        add_test(NAME "stdexec.Modules.${probe_definition}InternalsAreNotExported"
            COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}" "-DCONFIG=$<CONFIG>"
                "-DPROBE_TARGET=${probe_target}" "-DPRIVATE_NAME=${private_name}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/check_module_privacy.cmake")
        set_tests_properties("stdexec.Modules.${probe_definition}InternalsAreNotExported"
            PROPERTIES LABELS stdexec_verify RUN_SERIAL TRUE TIMEOUT 60)
    endforeach()
endif()
gtest_discover_tests(async_stdexec_verify_tests DISCOVERY_MODE PRE_TEST NO_PRETTY_VALUES TEST_PREFIX "stdexec."
    PROPERTIES LABELS stdexec_verify TIMEOUT 30)
add_custom_target(async_stdexec_verify
    COMMAND "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -L stdexec_verify --output-on-failure
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS async_stdexec_verify_tests USES_TERMINAL)
set_target_properties(async_stdexec_verify PROPERTIES FOLDER "async/stdexec/tests/verify")
