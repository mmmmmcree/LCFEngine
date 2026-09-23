project(shader_toy)

add_subdirectory(utils)
add_subdirectory(event)
add_subdirectory(system_scheduler)
add_subdirectory(file_system)
add_subdirectory(task_system)
add_subdirectory(render_system)
add_subdirectory(app)

add_executable(${PROJECT_NAME}
    shader_toy_main.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        shader_toy_app
        shader_toy_system_scheduler
        shader_toy_file_system
        shader_toy_task_system
        shader_toy_render_system
        utilities_log
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
