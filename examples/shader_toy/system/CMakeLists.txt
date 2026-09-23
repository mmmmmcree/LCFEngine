add_library(shader_toy_system INTERFACE)

target_include_directories(shader_toy_system
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(shader_toy_system
    INTERFACE
        shader_toy_event
)
