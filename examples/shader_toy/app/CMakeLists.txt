add_library(shader_toy_app STATIC
    ShaderToyApp.cpp
    ShaderToyApp.h
)

target_include_directories(shader_toy_app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(shader_toy_app
    PUBLIC
        window
        shader_toy_system_scheduler
)
