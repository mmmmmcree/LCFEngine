add_library(shader_toy_render_system SHARED
    src/RenderSystem.cpp
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")
target_sources(shader_toy_render_system PRIVATE ${HEADERS})

target_include_directories(shader_toy_render_system
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(shader_toy_render_system
    PRIVATE
        shader_toy_system_scheduler
        vk_core
        shader_core
        imgui::imgui
)

if(WIN32)
    set_target_properties(shader_toy_render_system PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )
endif()
