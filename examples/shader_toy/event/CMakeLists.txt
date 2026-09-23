add_library(shader_toy_event INTERFACE)

find_package(readerwriterqueue CONFIG REQUIRED)

target_include_directories(shader_toy_event
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(shader_toy_event
    INTERFACE
        shader_toy_containers
        shader_toy_functional
        utilities_core
        readerwriterqueue::readerwriterqueue
)
