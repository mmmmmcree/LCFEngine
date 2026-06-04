set(config_target "profiling_config")

option(USE_TRACY "Enable Tracy profiler" ON)

add_library(${config_target} INTERFACE)

if (USE_TRACY)
    target_compile_definitions(${config_target} INTERFACE USE_TRACY=1)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(${config_target} INTERFACE Tracy::TracyClient)
endif()
