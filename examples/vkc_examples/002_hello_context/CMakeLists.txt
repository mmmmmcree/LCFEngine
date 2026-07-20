project(vkc_example_002_hello_context)

file(GLOB_RECURSE SOURCES "src/*.cpp")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        vk_core
        utilities_log
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_compile_definitions(${PROJECT_NAME} PRIVATE

)
