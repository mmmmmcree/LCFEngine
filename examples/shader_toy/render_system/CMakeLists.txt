set(RENDER_SYSTEM_TARGET shader_toy_render_system)

add_library(${RENDER_SYSTEM_TARGET} SHARED
    src/RenderSystem.cpp
    src/ShaderToyInstance.cpp
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")
target_sources(${RENDER_SYSTEM_TARGET} PRIVATE ${HEADERS})

target_include_directories(${RENDER_SYSTEM_TARGET}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(${RENDER_SYSTEM_TARGET}
    PUBLIC
        vk_core
    PRIVATE
        utilities_log
        shader_toy_system_scheduler
        shader_core
        imgui::imgui
)

target_compile_definitions(${RENDER_SYSTEM_TARGET}
    PRIVATE
        SHADER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/"
)

vkc_configure_probe(
    TARGET ${RENDER_SYSTEM_TARGET}
    CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/vkc_config.h"
)

target_include_directories(${RENDER_SYSTEM_TARGET} BEFORE PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/vkc_probe/${RENDER_SYSTEM_TARGET}"
)

if(WIN32)
    set_target_properties(${RENDER_SYSTEM_TARGET} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
    )
endif()
