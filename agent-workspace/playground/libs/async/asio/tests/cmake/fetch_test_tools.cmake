include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/test_tools.cmake")
if(LCF_ASYNC_ASIO_BUILD_VERIFY OR LCF_ASYNC_ASIO_BUILD_SANDBOX)
    async_test_dependency(ASIO GTEST GTest::gtest_main GTest googletest
        https://github.com/google/googletest.git 52eb8108c5bdec04579160ae17225d66034bd723)
endif()
if(LCF_ASYNC_ASIO_BUILD_BENCHMARKS)
    async_test_dependency(ASIO BENCHMARK benchmark::benchmark benchmark googlebenchmark
        https://github.com/google/benchmark.git 192ef10025eb2c4cdd392bc502f0c852196baa48)
endif()
