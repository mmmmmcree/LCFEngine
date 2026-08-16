project(vkc_example_004_hello_compat_swapchain)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME}
    ${SOURCES}
)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        vk_core
        window
        utilities_log
)

vkc_configure_probe(
    TARGET ${PROJECT_NAME}
    CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/vk_config.h"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
