project(vkc_example_005_hello_triangle)

add_library(${PROJECT_NAME} OBJECT)
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINKER_LANGUAGE CXX
)
vkc_configure_probe(
    TARGET ${PROJECT_NAME}
    CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/vkc_config.h"
)

set(PROBE_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/vkc_probe/${PROJECT_NAME}"
)
set(SOURCE_FILE src/005_hello_triangle_main.cpp)

function(add_triangle_example variant mode_definition)
    set(target ${PROJECT_NAME}_${variant})

    add_executable(${target} ${SOURCE_FILE})
    add_dependencies(${target} ${PROJECT_NAME})
    target_include_directories(${target} BEFORE
        PRIVATE
            "${PROBE_INCLUDE_DIR}"
    )
    target_link_libraries(${target}
        PRIVATE
            vkc_examples_common
            shader_core
            utilities_log
    )
    target_compile_definitions(${target}
        PRIVATE
            ${mode_definition}
            SHADER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
    )
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endfunction()

add_triangle_example(render_pass_pipeline VKCE_005_RENDER_PASS_PIPELINE)
add_triangle_example(dynamic_pipeline VKCE_005_DYNAMIC_RENDERING_PIPELINE)
add_triangle_example(shader_object VKCE_005_DYNAMIC_RENDERING_SHADER_OBJECT)
