async_stdexec_test_dependency(GTEST GTest::gtest_main GTest googletest
    https://github.com/google/googletest.git 52eb8108c5bdec04579160ae17225d66034bd723)
add_executable(async_stdexec_sandbox_tests sandbox.cpp)
target_link_libraries(async_stdexec_sandbox_tests PRIVATE lcf::async_stdexec GTest::gtest_main)
target_include_directories(async_stdexec_sandbox_tests PRIVATE ../common)
include(GoogleTest)
gtest_discover_tests(async_stdexec_sandbox_tests DISCOVERY_MODE PRE_TEST TEST_PREFIX "stdexec."
    PROPERTIES LABELS stdexec_sandbox)
add_custom_target(async_stdexec_sandbox
    COMMAND "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -L stdexec_sandbox --output-on-failure
    DEPENDS async_stdexec_sandbox_tests WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" USES_TERMINAL)
set_target_properties(async_stdexec_sandbox async_stdexec_sandbox_tests PROPERTIES FOLDER "async/stdexec/tests/sandbox")
