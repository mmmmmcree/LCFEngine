project(vkc_example_006_hello_descriptor_set)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME}
    ${SOURCES}
)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        vkc_examples_common
        shader_core
        utilities_log
)

vkc_configure_probe(
    TARGET ${PROJECT_NAME}
    CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/vkc_config.h"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        SHADER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
