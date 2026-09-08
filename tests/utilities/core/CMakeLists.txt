add_executable(utilities_core_resource_unit_tests resource_lease_test.cpp)
target_compile_features(utilities_core_resource_unit_tests PRIVATE cxx_std_23)
target_link_libraries(utilities_core_resource_unit_tests
    PRIVATE utilities_core GTest::gtest_main Threads::Threads)
gtest_discover_tests(utilities_core_resource_unit_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES TIMEOUT 30)
