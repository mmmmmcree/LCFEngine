add_library(shader_toy_containers INTERFACE)

target_include_directories(shader_toy_containers
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(tsl-robin-map CONFIG REQUIRED)

target_link_libraries(shader_toy_containers
    INTERFACE
        utilities_core
        tsl::robin_map
)
