add_library(vkc_examples_common INTERFACE)

target_include_directories(vkc_examples_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vkc_examples_common
    INTERFACE
        vk_core
        window
)
