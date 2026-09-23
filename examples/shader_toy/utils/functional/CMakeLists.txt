add_library(shader_toy_functional INTERFACE)

target_include_directories(shader_toy_functional
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(function2 CONFIG REQUIRED)

target_link_libraries(shader_toy_functional
    INTERFACE
        function2::function2
)
