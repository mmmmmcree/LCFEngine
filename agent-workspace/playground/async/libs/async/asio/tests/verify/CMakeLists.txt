add_executable(async_verify_tests
    chain_test.cpp
    error_handling_test.cpp
    chain_junction_test.cpp
    contracts_test.cpp
)
target_link_libraries(async_verify_tests PRIVATE lcf::async_asio GTest::gtest_main)
set_target_properties(async_verify_tests PROPERTIES
    CXX_EXTENSIONS OFF FOLDER "async/tests/verify")

include(GoogleTest)

if(LCF_ASYNC_USE_MODULES)
    target_sources(async_verify_tests PRIVATE header_bridge.cpp)
    add_library(async_module_private_probe OBJECT EXCLUDE_FROM_ALL module_private_probe.cpp)
    target_link_libraries(async_module_private_probe PRIVATE lcf::async_asio)
    set_target_properties(async_module_private_probe PROPERTIES FOLDER "async/tests/verify/internal")
    add_test(NAME Modules.InternalNamesAreNotExported
        COMMAND "${CMAKE_COMMAND}"
            "-DASYNC_VERIFY_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DASYNC_VERIFY_CONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/check_module_privacy.cmake")
    set_tests_properties(Modules.InternalNamesAreNotExported PROPERTIES
        LABELS "async;module" TIMEOUT 60 RUN_SERIAL TRUE)
endif()
gtest_discover_tests(async_verify_tests
    DISCOVERY_MODE PRE_TEST
    NO_PRETTY_VALUES
    PROPERTIES LABELS async TIMEOUT 30
)

add_custom_target(async_verify
    COMMAND "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" --output-on-failure
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS async_verify_tests
    USES_TERMINAL
    COMMENT "Running async verification"
)
set_target_properties(async_verify PROPERTIES FOLDER "async/tests/verify")
