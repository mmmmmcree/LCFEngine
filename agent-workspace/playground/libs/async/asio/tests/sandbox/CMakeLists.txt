add_executable(async_sandbox_tests chain_sandbox.cpp)
target_link_libraries(async_sandbox_tests PRIVATE lcf::async_asio GTest::gtest_main)
set_target_properties(async_sandbox_tests PROPERTIES
    CXX_EXTENSIONS OFF FOLDER "async/asio/tests/sandbox")

include(GoogleTest)
gtest_discover_tests(async_sandbox_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "async\;sandbox" TIMEOUT 30
)

add_custom_target(async_sandbox
    COMMAND "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" --output-on-failure
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS async_sandbox_tests
    USES_TERMINAL
    COMMENT "Running async sandbox"
)
set_target_properties(async_sandbox PROPERTIES FOLDER "async/asio/tests/sandbox")
