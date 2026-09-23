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
        shader_toy_file_system
        shader_toy_task_system
        shader_core
        utilities_log
)

target_compile_definitions(shader_toy_app
    PUBLIC
        SHADER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/"
        IMAGE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/examples/assets/images/"
)
