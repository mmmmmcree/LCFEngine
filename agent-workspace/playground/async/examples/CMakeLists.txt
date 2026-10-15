option(LCF_ASYNC_BUILD_EXAMPLE "Build the external async example" ON)

if(LCF_ASYNC_BUILD_EXAMPLE)
    add_subdirectory(async_example)
endif()
