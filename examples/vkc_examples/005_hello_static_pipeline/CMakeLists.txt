project(vkc_example_005_hello_static_pipeline)

file(GLOB_RECURSE SOURCES "src/*.cpp")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        vk_core
        shader_core
        window
        utilities_log
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
